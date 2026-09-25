#include <gdt.h>
#include <idt.h>
#include <math/fpu.h>
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

static void smp_ap_entry(struct limine_smp_info *info) {
    uint32_t cpu = (uint32_t)info->extra_argument;
    if (cpu >= cpu_count)
        for (;;)
            __asm__ volatile("hlt");

    gdt_activate();
    tss_load();
    idt_activate();
    enable_fpu();

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
    if (!response || response->cpu_count == 0 || response->cpu_count > SMP_MAX_CPUS)
        return false;

    cpu_count = (uint32_t)response->cpu_count;
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

    for (uint32_t i = 0; i < cpu_count; ++i) {
        if (response->cpus[i]->lapic_id == response->bsp_lapic_id)
            continue;
        while (!__atomic_load_n(&cpus[i].online, __ATOMIC_ACQUIRE))
            __asm__ volatile("pause");
    }

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

uint32_t smp_cpu_count(void) {
    return cpu_count;
}

bool smp_cpu_is_online(uint32_t cpu_index) {
    return cpu_index < cpu_count && __atomic_load_n(&cpus[cpu_index].online, __ATOMIC_ACQUIRE);
}
