#include <flanterm/flanterm.h>
#include <gdt.h>
#include <graphics.h>
#include <heap.h>
#include <idt.h>
#include <pit.h>
#include <smp.h>
#include <spinlock.h>

typedef struct {
    bool is_bsp;
    volatile uint32_t online;
    volatile uint32_t pending;
    volatile uint32_t complete;
    smp_call_fn_t fn;
    void *context;
} smp_cpu_t;

static smp_cpu_t cpus[SMP_MAX_CPUS];
static uint32_t cpu_count;
static spinlock_t dispatch_lock = SPINLOCK_INITIALIZER;
static bool cursor_blink_started;

#define CURSOR_BLINK_TICKS (PIT_TICKS_PER_SECOND / 2)

/*
 * This is deliberately a long-running AP function rather than a BSP timer
 * task. APs currently do not receive timer interrupts, so it polls the BSP's
 * PIT tick counter while leaving interrupts disabled on the AP. The selected
 * AP remains dedicated to this sample and cannot service smp_call() work.
 */
static void smp_cursor_blink(void *context) {
    (void)context;

    uint64_t next_toggle = __atomic_load_n(&pit_ticks, __ATOMIC_ACQUIRE) +
                           CURSOR_BLINK_TICKS;

    for (;;) {
        uint64_t now = __atomic_load_n(&pit_ticks, __ATOMIC_ACQUIRE);
        if (now < next_toggle) {
            __asm__ volatile("pause");
            continue;
        }

        /* This redraws under the console lock, so it is safe alongside BSP output. */
        terminal_toggle_cursor();

        /* Rebase after a pause so a delayed AP never toggles repeatedly. */
        next_toggle = now + CURSOR_BLINK_TICKS;
    }
}

/* AP startup must not log: the regular FPU setup path writes to the shared
 * logger while the BSP is still bringing the other CPUs online. */
static void smp_enable_fpu(void) {
    uint64_t cr4;
    const uint16_t control_word = 0x037f;

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x200;
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
    __asm__ volatile("fldcw %0" : : "m"(control_word));
}

static void smp_ap_entry(struct limine_smp_info *info) {
    uint32_t cpu = (uint32_t)info->extra_argument;
    if (cpu >= cpu_count)
        for (;;)
            __asm__ volatile("hlt");

    gdt_activate();
    idt_activate();
    smp_enable_fpu();

    /* Validate the shared heap before this core starts handling work. */
    (void)kheap_check();

    /* The BSP has already loaded the sole TSS descriptor.  ltr marks that
     * descriptor busy, so loading it again on an AP raises #GP.  APs do not
     * enable interrupts or enter a lower privilege level yet, therefore they
     * do not use the TSS.  Give every CPU a private TSS/GDT before enabling
     * either of those features on APs. */

    __atomic_store_n(&cpus[cpu].online, 1, __ATOMIC_RELEASE);

    /* APs poll their private mailbox. This intentionally avoids enabling
     * interrupts on an AP until the interrupt-controller and per-CPU
     * scheduler support exist. */
    for (;;) {
        if (!__atomic_load_n(&cpus[cpu].pending, __ATOMIC_ACQUIRE)) {
            __asm__ volatile("pause");
            continue;
        }

        smp_call_fn_t fn = cpus[cpu].fn;
        void *context = cpus[cpu].context;
        if (fn)
            fn(context);

        __atomic_store_n(&cpus[cpu].complete, 1, __ATOMIC_RELEASE);
        __atomic_store_n(&cpus[cpu].pending, 0, __ATOMIC_RELEASE);
    }
}

bool smp_init(struct limine_smp_response *response) {
    LOG_SCOPE();
    if (!response || !response->cpus || response->cpu_count == 0 || response->cpu_count > SMP_MAX_CPUS)
        return false;

    const uint32_t discovered_cpu_count = (uint32_t)response->cpu_count;

    /* Validate every bootloader-owned CPU-info pointer before publishing an
     * AP entry address.  Once an address is published, that AP may execute
     * concurrently with this function. */
    for (uint32_t i = 0; i < discovered_cpu_count; ++i) {
        if (!response->cpus[i])
            return false;
    }

    cpu_count = discovered_cpu_count;
    cursor_blink_started = false;
    info("Initializing SMP for %u CPU(s)", __FILE__, cpu_count);
    for (uint32_t i = 0; i < cpu_count; ++i) {
        cpus[i].online = 0;
        cpus[i].is_bsp = false;
        cpus[i].pending = 0;
        cpus[i].complete = 0;
        cpus[i].fn = null;
        cpus[i].context = null;

        if (response->cpus[i]->lapic_id == response->bsp_lapic_id) {
            cpus[i].is_bsp = true;
            cpus[i].online = 1;
            continue;
        }

        response->cpus[i]->extra_argument = i;
        __atomic_store_n(&response->cpus[i]->goto_address, smp_ap_entry, __ATOMIC_RELEASE);
    }

    uint32_t online_count = 0;
    for (uint32_t i = 0; i < cpu_count; ++i) {
        if (response->cpus[i]->lapic_id != response->bsp_lapic_id){
            while (!__atomic_load_n(&cpus[i].online, __ATOMIC_ACQUIRE))
                __asm__ volatile("pause");
        }

        ++online_count;
        info("CPU %u (LAPIC ID 0x%x)%s initialized", __FILE__, i,
             response->cpus[i]->lapic_id,
             cpus[i].is_bsp ? " [BSP]" : "");

    }

    info("SMP initialization complete: " green_color "%u" reset_color "/" red_color "%u " reset_color " CPU(s) " green_color "online" reset_color, __FILE__, online_count, cpu_count);

    return true;
}

bool smp_call(uint32_t cpu_index, smp_call_fn_t fn, void *context) {
    if (!fn || cpu_index >= cpu_count || cpus[cpu_index].is_bsp ||
        !__atomic_load_n(&cpus[cpu_index].online, __ATOMIC_ACQUIRE))
        return false;

    spinlock_lock(&dispatch_lock);
    if (__atomic_load_n(&cpus[cpu_index].pending, __ATOMIC_ACQUIRE)) {
        spinlock_unlock(&dispatch_lock);
        return false;
    }

    cpus[cpu_index].fn = fn;
    cpus[cpu_index].context = context;
    __atomic_store_n(&cpus[cpu_index].complete, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&cpus[cpu_index].pending, 1, __ATOMIC_RELEASE);

    while (!__atomic_load_n(&cpus[cpu_index].complete, __ATOMIC_ACQUIRE))
        __asm__ volatile("pause");

    spinlock_unlock(&dispatch_lock);
    return true;
}

bool smp_call_all(smp_call_fn_t fn, void *context) {
    if (!fn)
        return false;

    bool called = false;
    spinlock_lock(&dispatch_lock);
    for (uint32_t i = 0; i < cpu_count; ++i) {
        if (cpus[i].is_bsp || !__atomic_load_n(&cpus[i].online, __ATOMIC_ACQUIRE) ||
            __atomic_load_n(&cpus[i].pending, __ATOMIC_ACQUIRE))
            continue;

        cpus[i].fn = fn;
        cpus[i].context = context;
        __atomic_store_n(&cpus[i].complete, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&cpus[i].pending, 1, __ATOMIC_RELEASE);
        called = true;
    }

    for (uint32_t i = 0; i < cpu_count; ++i) {
        if (cpus[i].is_bsp || !__atomic_load_n(&cpus[i].pending, __ATOMIC_ACQUIRE))
            continue;
        while (!__atomic_load_n(&cpus[i].complete, __ATOMIC_ACQUIRE))
            __asm__ volatile("pause");
    }

    spinlock_unlock(&dispatch_lock);
    return called;
}

bool smp_start_cursor_blink(void) {
    spinlock_lock(&dispatch_lock);
    if (cursor_blink_started) {
        spinlock_unlock(&dispatch_lock);
        return false;
    }

    for (uint32_t i = 0; i < cpu_count; ++i) {
        if (cpus[i].is_bsp || !__atomic_load_n(&cpus[i].online, __ATOMIC_ACQUIRE) ||
            __atomic_load_n(&cpus[i].pending, __ATOMIC_ACQUIRE))
            continue;

        cpus[i].fn = smp_cursor_blink;
        cpus[i].context = null;
        __atomic_store_n(&cpus[i].complete, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&cpus[i].pending, 1, __ATOMIC_RELEASE);
        cursor_blink_started = true;
        spinlock_unlock(&dispatch_lock);
        return true;
    }

    spinlock_unlock(&dispatch_lock);
    return false;
}

uint32_t smp_cpu_count(void) {
    return cpu_count;
}

bool smp_cpu_is_online(uint32_t cpu_index) {
    return cpu_index < cpu_count && __atomic_load_n(&cpus[cpu_index].online, __ATOMIC_ACQUIRE);
}
