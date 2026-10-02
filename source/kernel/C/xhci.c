/**
 * @file xhci.c
 * @brief Minimal xHCI 1.2 controller, command, event, and transfer-ring support.
 */
#include <cc-asm.h>
#include <graphics.h>
#include <heap.h>
#include <memory.h>
#include <paging.h>
#include <pci.h>
#include <pit.h>
#include <xhci.h>

#define XHCI_CAPLENGTH 0x00
#define XHCI_HCSPARAMS1 0x04
#define XHCI_HCSPARAMS2 0x08
#define XHCI_HCCPARAMS1 0x10
#define XHCI_DBOFF 0x14
#define XHCI_RTSOFF 0x18

#define XHCI_USBCMD 0x00
#define XHCI_USBSTS 0x04
#define XHCI_PAGESIZE 0x08
#define XHCI_CRCR 0x18
#define XHCI_DCBAAP 0x30
#define XHCI_CONFIG 0x38
#define XHCI_PORTS 0x400

#define XHCI_CMD_RUN (1U << 0)
#define XHCI_CMD_RESET (1U << 1)
#define XHCI_CMD_INTE (1U << 2)
#define XHCI_STS_HCH (1U << 0)
#define XHCI_STS_HSE (1U << 2)
#define XHCI_STS_EINT (1U << 3)
#define XHCI_STS_CNR (1U << 11)

#define XHCI_TRB_CYCLE (1U << 0)
#define XHCI_TRB_TC (1U << 1)
#define XHCI_TRB_IOC (1U << 5)
#define XHCI_TRB_TYPE_SHIFT 10
#define XHCI_TRB_TYPE_MASK (0x3FU << XHCI_TRB_TYPE_SHIFT)
#define XHCI_TRB_LINK 6
#define XHCI_TRB_ENABLE_SLOT 9
#define XHCI_TRB_NOOP_COMMAND 23
#define XHCI_TRB_COMMAND_COMPLETION 33
#define XHCI_TRB_PORT_STATUS_CHANGE 34
#define XHCI_TRB_TRANSFER_EVENT 32
#define XHCI_TRB_NORMAL 1

#define XHCI_PORT_CCS (1U << 0)
#define XHCI_PORT_PED (1U << 1)
#define XHCI_PORT_PR (1U << 4)
#define XHCI_PORT_PP (1U << 9)
#define XHCI_PORT_CSC (1U << 17)
#define XHCI_PORT_PRC (1U << 21)
#define XHCI_PORT_CHANGE_MASK (0x7FU << 17)
#define XHCI_PORT_RW_MASK ((1U << 9) | (7U << 25))

#define XHCI_EVENT_RING_SIZE 256
#define XHCI_COMMAND_TIMEOUT 500000
#define XHCI_MSI_VECTOR 0x50
#define XHCI_APIC_SPURIOUS_VECTOR 0xFE
#define XHCI_APIC_BASE_MSR 0x1B
#define XHCI_APIC_SVR 0xF0
#define XHCI_APIC_EOI 0xB0
#define XHCI_PCI_CAP_MSI 0x05
#define XHCI_PCI_CAP_MSIX 0x11

typedef struct {
    volatile uint32_t iman;
    volatile uint32_t imod;
    volatile uint32_t erstsz;
    uint32_t reserved;
    volatile uint64_t erstba;
    volatile uint64_t erdp;
} xhci_interrupter_t;

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint32_t size;
    uint32_t reserved;
} xhci_erst_entry_t;

typedef struct {
    xhci_trb_t *trbs;
    uint64_t physical;
    uint16_t enqueue;
    uint16_t dequeue;
    uint8_t cycle;
} xhci_ring_t;

typedef struct {
    volatile uint32_t *cap;
    volatile uint32_t *op;
    volatile uint32_t *doorbells;
    xhci_interrupter_t *intr0;
    uint64_t bar_phys;
    uint64_t *dcbaa;
    uint64_t dcbaa_phys;
    uint64_t *scratchpad_array;
    uint64_t scratchpad_array_phys;
    xhci_ring_t command;
    xhci_trb_t *events;
    uint64_t events_phys;
    xhci_erst_entry_t *erst;
    uint64_t erst_phys;
    xhci_ring_t *transfer[256][32];
    volatile uint32_t *lapic;
    uint64_t pending_command_phys;
    uint8_t event_index;
    uint8_t event_cycle;
    uint8_t port_known[256];
    uint8_t command_done;
    uint8_t command_code;
    uint8_t command_slot;
    uint8_t max_slots;
    uint8_t max_ports;
    uint8_t context_size;
    uint8_t address_64;
    uint8_t interrupt_mode;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t initialized;
    uint16_t version;
} xhci_controller_t;

static xhci_controller_t controllers[XHCI_MAX_CONTROLLERS];
static uint8_t controller_count;

static void *xhci_dma_alloc(xhci_controller_t *ctrl, size_t bytes,
    uint64_t *physical) {
    size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t page = allocate_pages_contiguous(pages);
    if (!page)
        return NULL;
    if (!ctrl->address_64 && page + pages * PAGE_SIZE - 1U > UINT32_MAX)
        return NULL;
    *physical = page;
    return paging_phys_to_virt(page);
}

static int xhci_wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t value,
    uint32_t timeout_ms) {
    uint64_t start = pit_ticks;
    uint64_t timeout_ticks = (timeout_ms + 9U) / 10U;
    for (uint32_t i = 0; i < 10000000U; ++i) {
        if ((*reg & mask) == value)
            return 0;
        if (pit_ticks - start >= timeout_ticks)
            return -1;
        __asm__ volatile("pause");
    }
    return -1;
}

static const char *xhci_completion_name(uint8_t code) {
    switch (code) {
        case 1:
            return "Success";
        case 2:
            return "Data Buffer Error";
        case 3:
            return "Babble Detected";
        case 4:
            return "USB Transaction Error";
        case 5:
            return "TRB Error";
        case 6:
            return "Stall Error";
        case 7:
            return "Resource Error";
        case 8:
            return "Bandwidth Error";
        case 9:
            return "No Slots Available";
        case 10:
            return "Invalid Stream Type";
        case 11:
            return "Slot Not Enabled";
        case 12:
            return "Endpoint Not Enabled";
        case 13:
            return "Short Packet";
        case 14:
            return "Ring Underrun";
        case 15:
            return "Ring Overrun";
        case 16:
            return "VF Event Ring Full";
        case 17:
            return "Parameter Error";
        case 18:
            return "Bandwidth Overrun";
        case 19:
            return "Context State Error";
        case 20:
            return "No Ping Response";
        case 21:
            return "Event Ring Full";
        case 22:
            return "Incompatible Device";
        case 23:
            return "Missed Service";
        case 24:
            return "Command Ring Stopped";
        case 25:
            return "Command Aborted";
        case 26:
            return "Stopped";
        case 27:
            return "Stopped - Length Invalid";
        case 28:
            return "Stopped - Short Packet";
        case 29:
            return "Max Exit Latency Too Large";
        default:
            return "Unknown Completion Code";
    }
}

static void xhci_ring_command_doorbell(xhci_controller_t *ctrl) {
    ctrl->doorbells[0] = 0;
}

int xhci_ring_doorbell(uint8_t slot_id, uint8_t target) {
    if (controller_count == 0)
        return -1;
    xhci_controller_t *ctrl = &controllers[0];
    if (!ctrl->initialized || slot_id > ctrl->max_slots || target > 31 ||
        (slot_id == 0 && target != 0))
        return -1;
    ctrl->doorbells[slot_id] = target;
    return 0;
}

static void xhci_process_event(xhci_controller_t *ctrl, const xhci_trb_t *event) {
    uint8_t type = (uint8_t)((event->control & XHCI_TRB_TYPE_MASK) >>
                             XHCI_TRB_TYPE_SHIFT);
    uint8_t code = (uint8_t)(event->status >> 24);
    uint8_t slot = (uint8_t)(event->control >> 24);

    if (type == XHCI_TRB_COMMAND_COMPLETION) {
        uint64_t command_phys = event->parameter & ~0xFULL;
        info("xHCI command completion: %s (%u), slot %u", __FILE__,
            xhci_completion_name(code), code, slot);
        if (command_phys == ctrl->pending_command_phys) {
            ctrl->command_code = code;
            ctrl->command_slot = slot;
            ctrl->command_done = 1;
        }
        if (code == 1)
            done("xHCI command completed successfully", __FILE__);
        else
            error("xHCI command failed: %s (%u)", __FILE__,
                xhci_completion_name(code), code);
    } else if (type == XHCI_TRB_PORT_STATUS_CHANGE) {
        uint8_t port_id = (uint8_t)(event->parameter >> 24);
        info("xHCI port %u status-change event", __FILE__, port_id);
    } else if (type == XHCI_TRB_TRANSFER_EVENT) {
        uint8_t endpoint = (uint8_t)((event->control >> 16) & 0x1FU);
        uint64_t trb_phys = event->parameter & ~0xFULL;
        if (slot && endpoint && ctrl->transfer[slot][endpoint]) {
            xhci_ring_t *ring = ctrl->transfer[slot][endpoint];
            if (trb_phys >= ring->physical &&
                trb_phys < ring->physical +
                               (XHCI_TRB_RING_ENTRIES - 1U) * sizeof(xhci_trb_t)) {
                ring->dequeue = (uint16_t)(((trb_phys - ring->physical) /
                                                   sizeof(xhci_trb_t) +
                                               1U) %
                                           (XHCI_TRB_RING_ENTRIES - 1U));
            }
        }
        info("xHCI transfer completion: %s (%u), slot %u endpoint %u", __FILE__,
            xhci_completion_name(code), code, slot,
            endpoint);
        if (code != 1 && code != 13)
            warn("xHCI transfer completed with a non-success code", __FILE__);
    } else {
        warn("xHCI event type %u completion %s (%u)", __FILE__, type,
            xhci_completion_name(code), code);
    }
}

/* xHCI 1.2 §4.8-§4.9: event-ring dequeue and cycle-state handling. */
static void xhci_poll_events(xhci_controller_t *ctrl) {
    if (!ctrl->initialized)
        return;
    uint8_t consumed = 0;
    for (uint16_t count = 0; count < XHCI_EVENT_RING_SIZE; ++count) {
        xhci_trb_t *event = &ctrl->events[ctrl->event_index];
        uint32_t event_control = __atomic_load_n(&event->control, __ATOMIC_ACQUIRE);
        if ((event_control & XHCI_TRB_CYCLE) != ctrl->event_cycle)
            break;

        xhci_process_event(ctrl, event);
        consumed = 1;
        if (++ctrl->event_index == XHCI_EVENT_RING_SIZE) {
            ctrl->event_index = 0;
            ctrl->event_cycle ^= 1;
        }
    }

    if (consumed)
        ctrl->intr0->erdp = ctrl->events_phys +
                                (uint64_t)ctrl->event_index * sizeof(xhci_trb_t) |
                            (1U << 3);
    ctrl->intr0->iman = (ctrl->intr0->iman & (1U << 1)) | 1U;
    ctrl->op[XHCI_USBSTS / 4] = XHCI_STS_EINT;
}

/* xHCI 1.2 §5.4: PORTSC change bits are write-one-to-clear. */
static void xhci_poll_ports(xhci_controller_t *ctrl) {
    for (uint16_t port = 1; port <= ctrl->max_ports; ++port) {
        volatile uint32_t *portsc = &ctrl->op[(XHCI_PORTS / 4) + ((port - 1U) * 4U)];
        uint32_t status = *portsc;
        uint8_t changed = (status & XHCI_PORT_CHANGE_MASK) != 0;
        uint8_t first_scan = !ctrl->port_known[port];
        ctrl->port_known[port] = 1;
        if (!changed && !first_scan)
            continue;

        if ((status & XHCI_PORT_CSC) || first_scan) {
            if (status & XHCI_PORT_CCS)
                info("xHCI port %u connected (speed ID %u)", __FILE__, port,
                    (status >> 10) & 0xFU);
            else if (status & XHCI_PORT_CSC)
                info("xHCI port %u disconnected", __FILE__, port);
        }
        if (status & (XHCI_PORT_PRC | (1U << 19)))
            info("xHCI port %u reset completed; enabled=%u", __FILE__, port,
                !!(status & XHCI_PORT_PED));

        *portsc = (status & XHCI_PORT_RW_MASK) | (status & XHCI_PORT_CHANGE_MASK);
        if ((first_scan || (status & XHCI_PORT_CSC)) &&
            (status & XHCI_PORT_CCS) && !(status & XHCI_PORT_PED) &&
            !(status & (XHCI_PORT_PR | (1U << 31)))) {
            info("Resetting connected xHCI port %u", __FILE__, port);
            uint32_t reset = ((status >> 10) & 0xFU) >= 4U ? (1U << 31) : XHCI_PORT_PR;
            *portsc = (status & XHCI_PORT_RW_MASK) |
                      (status & XHCI_PORT_CHANGE_MASK) | reset;
        }
    }
}

static void xhci_poll_controller(xhci_controller_t *ctrl) {
    if (!ctrl->initialized)
        return;
    xhci_poll_events(ctrl);
    xhci_poll_ports(ctrl);
}

static void xhci_spurious_interrupt_handler(InterruptFrame *frame) {
    (void)frame;
}

void xhci_poll(void) {
    LOG_SCOPE();
    for (uint8_t i = 0; i < controller_count; ++i)
        xhci_poll_controller(&controllers[i]);
}

static int xhci_apic_setup(xhci_controller_t *ctrl) {
    uint32_t eax = 1, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if (!(edx & (1U << 9)))
        return -1;

    uint64_t apic_base = rdmsr64(XHCI_APIC_BASE_MSR);
    if (apic_base & (1ULL << 10))
        return -1;
    apic_base |= 1ULL << 11;
    wrmsr64(XHCI_APIC_BASE_MSR, apic_base);
    ctrl->lapic = paging_phys_to_virt((uintptr_t)(apic_base & 0x0000000FFFFFF000ULL));
    registerInterruptHandler(XHCI_APIC_SPURIOUS_VECTOR,
        xhci_spurious_interrupt_handler);
    ctrl->lapic[XHCI_APIC_SVR / 4] =
        (ctrl->lapic[XHCI_APIC_SVR / 4] & 0xFFFFFF00U) |
        XHCI_APIC_SPURIOUS_VECTOR | (1U << 8);
    return 0;
}

static uint32_t xhci_pci_read8(uint8_t bus, uint8_t device, uint8_t function,
    uint8_t offset) {
    return (pci_config_read_dword(bus, device, function, offset & 0xFCU) >>
               ((offset & 3U) * 8U)) &
           0xFFU;
}

static uint32_t xhci_pci_read16(uint8_t bus, uint8_t device, uint8_t function,
    uint8_t offset) {
    return (pci_config_read_dword(bus, device, function, offset & 0xFCU) >>
               ((offset & 2U) * 8U)) &
           0xFFFFU;
}

static void xhci_pci_write16(uint8_t bus, uint8_t device, uint8_t function,
    uint8_t offset, uint16_t value) {
    uint8_t aligned = offset & 0xFCU;
    uint32_t shift = (offset & 2U) * 8U;
    uint32_t reg = pci_config_read_dword(bus, device, function, aligned);
    reg = (reg & ~(0xFFFFU << shift)) | ((uint32_t)value << shift);
    pci_config_write_dword(bus, device, function, aligned, reg);
}

static void xhci_pci_write_command(uint8_t bus, uint8_t device, uint8_t function,
    uint16_t value) {
    pci_config_write_dword(bus, device, function, 0x04, value);
}

static uint8_t xhci_find_capability(xhci_controller_t *ctrl, uint8_t id) {
    uint8_t pointer = (uint8_t)(xhci_pci_read8(ctrl->bus, ctrl->device,
                                    ctrl->function, 0x34) &
                                ~3U);
    for (uint8_t count = 0; pointer >= 0x40 && count < 48; ++count) {
        uint32_t header = xhci_pci_read8(ctrl->bus, ctrl->device, ctrl->function, pointer);
        uint8_t next = (uint8_t)(xhci_pci_read8(ctrl->bus, ctrl->device,
                                     ctrl->function, pointer + 1U) &
                                 ~3U);
        if ((uint8_t)header == id)
            return pointer;
        if (next == pointer)
            break;
        pointer = next;
    }
    return 0;
}

static uint64_t xhci_get_bar(uint8_t bus, uint8_t device, uint8_t function,
    uint8_t index, int *is_64bit) {
    if (index > 5)
        return 0;
    if (index > 0) {
        uint32_t previous = pci_config_read_dword(
            bus, device, function, (uint8_t)(0x10U + (index - 1U) * 4U));
        if (!(previous & 1U) && ((previous >> 1) & 3U) == 2U)
            return 0;
    }
    uint8_t offset = (uint8_t)(0x10U + index * 4U);
    uint32_t low = pci_config_read_dword(bus, device, function, offset);
    if (low == 0xFFFFFFFFU || (low & 1U))
        return 0;
    uint32_t type = (low >> 1) & 3U;
    *is_64bit = (type == 2U);
    uint64_t base = low & ~0xFULL;
    if (*is_64bit) {
        if (index >= 5)
            return 0;
        base |= (uint64_t)pci_config_read_dword(bus, device, function,
                    offset + 4U)
                << 32;
    }
    return base;
}

static int xhci_enable_msix(xhci_controller_t *ctrl) {
    uint8_t cap = xhci_find_capability(ctrl, XHCI_PCI_CAP_MSIX);
    if (!cap)
        return -1;

    uint16_t control = (uint16_t)xhci_pci_read16(ctrl->bus, ctrl->device,
        ctrl->function, cap + 2U);
    uint32_t table = pci_config_read_dword(ctrl->bus, ctrl->device, ctrl->function,
        cap + 4U);
    uint8_t bir = table & 7U;
    int is_64bit = 0;
    uint64_t bar = xhci_get_bar(ctrl->bus, ctrl->device, ctrl->function,
        bir, &is_64bit);
    if (!bar)
        return -1;

    volatile uint32_t *entry =
        paging_phys_to_virt((uintptr_t)(bar + (table & ~7U)));
    uint32_t old_control = entry[3];
    entry[3] = old_control | 1U;
    entry[0] = 0xFEE00000U | ((uint32_t)(ctrl->lapic[0x20 / 4] >> 24) << 12);
    entry[1] = 0;
    entry[2] = XHCI_MSI_VECTOR;
    entry[3] = old_control & ~1U;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    control |= (1U << 15) | (1U << 14);
    xhci_pci_write16(ctrl->bus, ctrl->device, ctrl->function, cap + 2U, control);
    xhci_pci_write16(ctrl->bus, ctrl->device, ctrl->function, cap + 2U,
        (uint16_t)(control & ~(1U << 14)));
    xhci_pci_write_command(ctrl->bus, ctrl->device, ctrl->function,
        (uint16_t)(xhci_pci_read16(ctrl->bus, ctrl->device,
                       ctrl->function, 0x04) |
                   (1U << 10)));
    registerInterruptHandler(XHCI_MSI_VECTOR, xhci_interrupt_handler);
    ctrl->interrupt_mode = 2;
    info("xHCI MSI-X enabled (%u table entries)", __FILE__,
        (control & 0x7FFU) + 1U);
    return 0;
}

static int xhci_enable_msi(xhci_controller_t *ctrl) {
    uint8_t cap = xhci_find_capability(ctrl, XHCI_PCI_CAP_MSI);
    if (!cap)
        return -1;

    uint16_t control = (uint16_t)xhci_pci_read16(ctrl->bus, ctrl->device,
        ctrl->function, cap + 2U);
    uint8_t data_offset = (control & (1U << 7)) ? 12U : 8U;
    uint32_t address = 0xFEE00000U |
                       ((uint32_t)(ctrl->lapic[0x20 / 4] >> 24) << 12);
    pci_config_write_dword(ctrl->bus, ctrl->device, ctrl->function, cap + 4U, address);
    if (control & (1U << 7)) {
        pci_config_write_dword(ctrl->bus, ctrl->device, ctrl->function, cap + 8U, 0);
        data_offset = 12;
    }
    xhci_pci_write16(ctrl->bus, ctrl->device, ctrl->function, cap + data_offset,
        XHCI_MSI_VECTOR);
    control &= (uint16_t)~(7U << 4);
    control |= 1U;
    xhci_pci_write16(ctrl->bus, ctrl->device, ctrl->function, cap + 2U, control);
    xhci_pci_write_command(ctrl->bus, ctrl->device, ctrl->function,
        (uint16_t)(xhci_pci_read16(ctrl->bus, ctrl->device,
                       ctrl->function, 0x04) |
                   (1U << 10)));
    registerInterruptHandler(XHCI_MSI_VECTOR, xhci_interrupt_handler);
    ctrl->interrupt_mode = 1;
    info("xHCI MSI enabled", __FILE__);
    return 0;
}

/* PCI MSI/MSI-X use the local xAPIC destination; otherwise xhci_poll() is the fallback. */
static void xhci_configure_interrupts(xhci_controller_t *ctrl) {
    if (xhci_apic_setup(ctrl) != 0) {
        warn("xHCI MSI/MSI-X unavailable; using polling (xAPIC unavailable)", __FILE__);
        return;
    }
    if (xhci_enable_msix(ctrl) == 0 || xhci_enable_msi(ctrl) == 0) {
        ctrl->intr0->iman = 3U;
        ctrl->op[XHCI_USBCMD / 4] |= XHCI_CMD_INTE;
        return;
    }
    warn("xHCI MSI/MSI-X capability unavailable; using polling", __FILE__);
    ctrl->intr0->iman = 1U;
}

static int xhci_alloc_controller_memory(xhci_controller_t *ctrl) {
    /* xHCI 1.2 §4.5: DCBAA, scratchpad buffers, and ring memory are DMA-visible. */
    ctrl->dcbaa = xhci_dma_alloc(ctrl, PAGE_SIZE, &ctrl->dcbaa_phys);
    ctrl->command.trbs = xhci_dma_alloc(ctrl, PAGE_SIZE, &ctrl->command.physical);
    ctrl->events = xhci_dma_alloc(ctrl, PAGE_SIZE, &ctrl->events_phys);
    ctrl->erst = xhci_dma_alloc(ctrl, PAGE_SIZE, &ctrl->erst_phys);
    if (!ctrl->dcbaa || !ctrl->command.trbs || !ctrl->events || !ctrl->erst)
        return -1;

    ctrl->command.cycle = 1;
    ctrl->event_cycle = 1;
    ctrl->command.trbs[XHCI_TRB_RING_ENTRIES - 1U].parameter =
        ctrl->command.physical;
    ctrl->command.trbs[XHCI_TRB_RING_ENTRIES - 1U].control =
        (XHCI_TRB_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC |
        ctrl->command.cycle;

    uint32_t hcs2 = ctrl->cap[XHCI_HCSPARAMS2 / 4];
    uint32_t scratchpad_count = (((hcs2 >> 21) & 0x1FU) << 5) |
                                ((hcs2 >> 27) & 0x1FU);
    if (scratchpad_count) {
        ctrl->scratchpad_array = xhci_dma_alloc(ctrl, (size_t)scratchpad_count * sizeof(uint64_t),
            &ctrl->scratchpad_array_phys);
        if (!ctrl->scratchpad_array)
            return -1;
        for (uint32_t i = 0; i < scratchpad_count; ++i) {
            uint64_t scratch_phys;
            void *scratch = xhci_dma_alloc(ctrl, PAGE_SIZE, &scratch_phys);
            if (!scratch)
                return -1;
            ctrl->scratchpad_array[i] = scratch_phys;
        }
        ctrl->dcbaa[0] = ctrl->scratchpad_array_phys;
    }

    ctrl->erst[0].address = ctrl->events_phys;
    ctrl->erst[0].size = XHCI_EVENT_RING_SIZE;
    ctrl->erst[0].reserved = 0;
    return 0;
}

/* xHCI 1.2 Extended Capabilities: BIOS/OS ownership hand-off via USBLEGSUP. */
static int xhci_handoff_legacy(xhci_controller_t *ctrl) {
    uint32_t offset = (ctrl->cap[XHCI_HCCPARAMS1 / 4] >> 16) * 4U;
    for (uint16_t count = 0; offset && count < 256; ++count) {
        volatile uint32_t *extended = (volatile uint32_t *)((uintptr_t)ctrl->cap + offset);
        uint32_t header = *extended;
        if ((header & 0xFFU) == 1U) {
            *extended = header | (1U << 24);
            uint64_t start = pit_ticks;
            for (uint32_t wait = 0; wait < 10000000U; ++wait) {
                if (!(*extended & (1U << 16)))
                    return 0;
                if (pit_ticks - start >= 500U)
                    break;
                __asm__ volatile("pause");
            }
            error("xHCI BIOS ownership hand-off timed out", __FILE__);
            return -1;
        }
        uint8_t next = (uint8_t)(header >> 8);
        if (!next)
            break;
        offset += (uint32_t)next * 4U;
    }
    return 0;
}

static int xhci_start_controller(xhci_controller_t *ctrl) {
    volatile uint32_t *usbcmd = &ctrl->op[XHCI_USBCMD / 4];
    volatile uint32_t *usbsts = &ctrl->op[XHCI_USBSTS / 4];
    info("xHCI acquiring controller ownership", __FILE__);
    if (xhci_handoff_legacy(ctrl) != 0)
        return -1;
    done("xHCI controller ownership acquired", __FILE__);
    if (!(*usbsts & XHCI_STS_HCH)) {
        *usbcmd &= ~XHCI_CMD_RUN;
        if (xhci_wait_bits(usbsts, XHCI_STS_HCH, XHCI_STS_HCH, 1000) != 0) {
            error("xHCI controller did not halt", __FILE__);
            return -1;
        }
    }

    *usbsts = 0xFFFFFFFFU;
    *usbcmd = (*usbcmd & ~(XHCI_CMD_RUN | XHCI_CMD_INTE)) | XHCI_CMD_RESET;
    if (xhci_wait_bits(usbcmd, XHCI_CMD_RESET, 0, 1000) != 0 ||
        xhci_wait_bits(usbsts, XHCI_STS_CNR, 0, 1000) != 0) {
        error("xHCI controller reset timed out", __FILE__);
        return -1;
    }

    if ((ctrl->op[XHCI_PAGESIZE / 4] & 1U) == 0) {
        error("xHCI controller does not support 4 KiB pages", __FILE__);
        return -1;
    }
    if (xhci_alloc_controller_memory(ctrl) != 0) {
        error("Unable to allocate xHCI DMA structures", __FILE__);
        return -1;
    }

    ctrl->op[XHCI_DCBAAP / 4] = (uint32_t)ctrl->dcbaa_phys;
    ctrl->op[(XHCI_DCBAAP + 4) / 4] = (uint32_t)(ctrl->dcbaa_phys >> 32);
    ctrl->op[XHCI_CRCR / 4] = (uint32_t)ctrl->command.physical | 1U;
    ctrl->op[(XHCI_CRCR + 4) / 4] = (uint32_t)(ctrl->command.physical >> 32);
    /* xHCI 1.2 §5.5: interrupter 0 event-ring segment table and dequeue pointer. */
    ctrl->intr0->erstsz = 1;
    ctrl->intr0->erstba = ctrl->erst_phys;
    ctrl->intr0->erdp = ctrl->events_phys;
    ctrl->intr0->iman = 0;
    ctrl->op[XHCI_CONFIG / 4] = ctrl->max_slots;
    ctrl->initialized = 1;

    *usbcmd = (*usbcmd & ~XHCI_CMD_INTE) | XHCI_CMD_RUN;
    if (xhci_wait_bits(usbsts, XHCI_STS_HCH, 0, 1000) != 0) {
        ctrl->initialized = 0;
        error("xHCI controller failed to start", __FILE__);
        return -1;
    }
    if (*usbsts & XHCI_STS_HSE) {
        ctrl->initialized = 0;
        error("xHCI host system error during startup", __FILE__);
        return -1;
    }
    done("xHCI controller running; %u slots, %u ports, context size %u",
        __FILE__, ctrl->max_slots, ctrl->max_ports, ctrl->context_size);
    xhci_configure_interrupts(ctrl);
    return 0;
}

/* xHCI 1.2 §4.9.2: command TRBs, Link TRB, producer cycle, doorbell 0. */
int xhci_submit_command(uint8_t command_type, uint32_t parameter_low,
    uint32_t parameter_high, uint32_t control,
    xhci_completion_t *completion) {
    LOG_SCOPE();
    if (!controller_count || !controllers[0].initialized)
        return -1;
    xhci_controller_t *ctrl = &controllers[0];
    if (ctrl->command.enqueue >= XHCI_TRB_RING_ENTRIES - 1U)
        ctrl->command.enqueue = 0;

    xhci_trb_t *trb = &ctrl->command.trbs[ctrl->command.enqueue];
    trb->parameter = ((uint64_t)parameter_high << 32) | parameter_low;
    trb->status = 0;
    trb->control = (control & ~(XHCI_TRB_TYPE_MASK | XHCI_TRB_CYCLE)) |
                   ((uint32_t)command_type << XHCI_TRB_TYPE_SHIFT) |
                   ctrl->command.cycle;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    ctrl->pending_command_phys = ctrl->command.physical +
                                 (uint64_t)ctrl->command.enqueue * sizeof(xhci_trb_t);
    ctrl->command_done = 0;
    if (++ctrl->command.enqueue == XHCI_TRB_RING_ENTRIES - 1U) {
        ctrl->command.enqueue = 0;
        ctrl->command.trbs[XHCI_TRB_RING_ENTRIES - 1U].control =
            (XHCI_TRB_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC |
            ctrl->command.cycle;
        ctrl->command.cycle ^= 1U;
    }
    xhci_ring_command_doorbell(ctrl);

    for (uint32_t i = 0; i < XHCI_COMMAND_TIMEOUT; ++i) {
        xhci_poll_events(ctrl);
        if (ctrl->command_done) {
            if (completion) {
                completion->slot_id = ctrl->command_slot;
                completion->completion_code = ctrl->command_code;
                completion->endpoint_id = 0;
                completion->residual_length = 0;
            }
            return ctrl->command_code == 1 ? 0 : -2;
        }
        __asm__ volatile("pause");
    }
    error("xHCI command timed out (type %u)", __FILE__, command_type);
    ctrl->pending_command_phys = 0;
    return -3;
}

int xhci_enable_slot(uint8_t *slot_id) {
    xhci_completion_t completion;
    int result = xhci_submit_command(XHCI_TRB_ENABLE_SLOT, 0, 0, 0, &completion);
    if (result == 0 && completion.slot_id == 0) {
        error("xHCI Enable Slot returned an invalid slot ID", __FILE__);
        return -2;
    }
    if (result == 0 && slot_id)
        *slot_id = completion.slot_id;
    return result;
}

/* xHCI 1.2 §4.11: endpoint transfer rings; §4.6: slot/endpoint doorbells. */
int xhci_create_transfer_ring(uint8_t slot_id, uint8_t endpoint_id,
    uint64_t *dequeue_pointer) {
    LOG_SCOPE();
    if (!controller_count || !controllers[0].initialized || slot_id == 0 ||
        slot_id > controllers[0].max_slots || endpoint_id == 0 ||
        endpoint_id > 31 || !dequeue_pointer)
        return -1;
    xhci_controller_t *ctrl = &controllers[0];
    xhci_ring_t *ring = ctrl->transfer[slot_id][endpoint_id];
    if (!ring) {
        ring = kmalloc(sizeof(*ring));
        if (!ring)
            return -1;
        memset(ring, 0, sizeof(*ring));
        ring->trbs = xhci_dma_alloc(ctrl, PAGE_SIZE, &ring->physical);
        if (!ring->trbs) {
            kfree(ring);
            return -1;
        }
        ring->cycle = 1;
        ring->trbs[XHCI_TRB_RING_ENTRIES - 1U].parameter = ring->physical;
        ring->trbs[XHCI_TRB_RING_ENTRIES - 1U].control =
            (XHCI_TRB_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC |
            ring->cycle;
        ctrl->transfer[slot_id][endpoint_id] = ring;
    }
    *dequeue_pointer = ring->physical | ring->cycle;
    return 0;
}

int xhci_queue_transfer(uint8_t slot_id, uint8_t endpoint_id,
    void *buffer, uint32_t length) {
    LOG_SCOPE();
    if (!controller_count || !controllers[0].initialized || slot_id == 0 ||
        slot_id > controllers[0].max_slots || endpoint_id == 0 ||
        endpoint_id > 31 || !buffer || length == 0 || length > 0x1FFFFU)
        return -1;
    xhci_controller_t *ctrl = &controllers[0];
    xhci_ring_t *ring = ctrl->transfer[slot_id][endpoint_id];
    if (!ring)
        return -1;

    xhci_trb_t *trb = &ring->trbs[ring->enqueue];
    uint16_t next = (uint16_t)(ring->enqueue + 1U);
    if (next == XHCI_TRB_RING_ENTRIES - 1U)
        next = 0;
    if (next == ring->dequeue)
        return -2;
    uint64_t buffer_phys = fast_virt_to_phys(buffer);
    if (!ctrl->address_64 &&
        buffer_phys + (uint64_t)length - 1U > UINT32_MAX)
        return -1;
    if (buffer_phys > UINT64_MAX - ((uint64_t)length - 1U))
        return -1;
    trb->parameter = buffer_phys;
    trb->status = (length & 0x1FFFFU);
    trb->control = (XHCI_TRB_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                   XHCI_TRB_IOC | ring->cycle;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    if (++ring->enqueue == XHCI_TRB_RING_ENTRIES - 1U) {
        ring->enqueue = 0;
        ring->trbs[XHCI_TRB_RING_ENTRIES - 1U].control =
            (XHCI_TRB_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC | ring->cycle;
        ring->cycle ^= 1U;
    }
    return xhci_ring_doorbell(slot_id, endpoint_id);
}

void xhci_interrupt_handler(InterruptFrame *frame) {
    (void)frame;
    xhci_poll();
    if (controller_count && controllers[0].lapic)
        controllers[0].lapic[XHCI_APIC_EOI / 4] = 0;
}

void probe_xhci(uint8_t bus, uint8_t slot, uint8_t function) {
    LOG_SCOPE();
    for (uint8_t i = 0; i < controller_count; ++i) {
        xhci_controller_t *old = &controllers[i];
        if (old->bus == bus && old->device == slot && old->function == function)
            return;
    }
    if (controller_count >= XHCI_MAX_CONTROLLERS) {
        warn("xHCI controller limit reached; skipping %02x:%02x.%u",
            __FILE__, bus, slot, function);
        return;
    }

    uint16_t command = (uint16_t)pci_config_read_dword(bus, slot, function, 0x04);
    xhci_pci_write_command(bus, slot, function, (uint16_t)(command | 0x0006U));

    uint32_t bar_low = pci_config_read_dword(bus, slot, function, 0x10);
    uint32_t bar_type = (bar_low >> 1) & 3U;
    if ((bar_low & 1U) || bar_low == 0xFFFFFFFFU ||
        bar_type == 1U || bar_type == 3U) {
        error("Invalid xHCI BAR0", __FILE__);
        return;
    }
    uint64_t bar = bar_low & ~0xFULL;
    if (bar_type == 2U)
        bar |= (uint64_t)pci_config_read_dword(bus, slot, function, 0x14) << 32;
    if (!bar) {
        error("xHCI BAR0 is not assigned", __FILE__);
        return;
    }

    xhci_controller_t *ctrl = &controllers[controller_count];
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->bus = bus;
    ctrl->device = slot;
    ctrl->function = function;
    ctrl->bar_phys = bar;
    ctrl->cap = paging_phys_to_virt((uintptr_t)bar);
    uint8_t cap_length = (uint8_t)ctrl->cap[XHCI_CAPLENGTH / 4];
    ctrl->version = (uint16_t)(ctrl->cap[XHCI_CAPLENGTH / 4] >> 16);
    if (cap_length < 0x20 || cap_length >= 0x100) {
        error("xHCI capability length is invalid: 0x%x", __FILE__, cap_length);
        return;
    }
    /* xHCI 1.2 §§5.3-5.6: capability, operational, runtime, and doorbell blocks. */
    uint32_t hcs1 = ctrl->cap[XHCI_HCSPARAMS1 / 4];
    ctrl->max_slots = hcs1 & 0xFFU;
    ctrl->max_ports = hcs1 >> 24;
    uint32_t hcc1 = ctrl->cap[XHCI_HCCPARAMS1 / 4];
    ctrl->context_size = (hcc1 & (1U << 2)) ? 64 : 32;
    ctrl->address_64 = hcc1 & 1U;
    uint32_t dboff = ctrl->cap[XHCI_DBOFF / 4] & ~3U;
    uint32_t rtsoff = ctrl->cap[XHCI_RTSOFF / 4] & ~0x1FU;
    ctrl->op = (volatile uint32_t *)((uintptr_t)ctrl->cap + cap_length);
    ctrl->doorbells = (volatile uint32_t *)((uintptr_t)ctrl->cap + dboff);
    ctrl->intr0 = (xhci_interrupter_t *)((uintptr_t)ctrl->cap + rtsoff + 0x20);
    if (!ctrl->max_slots || !ctrl->max_ports || dboff >= 0x1000000 ||
        rtsoff >= 0x1000000) {
        error("xHCI capability registers contain invalid limits or offsets", __FILE__);
        return;
    }

    info("xHCI version 0x%04x, %u slots, %u ports, BAR0 0x%X", __FILE__,
        ctrl->version, (uint32_t)(hcs1 & 0xFFU), ctrl->max_ports, (uint32_t)bar);
    info("xHCI HCCPARAMS1=0x%X, DBOFF=0x%X, RTSOFF=0x%X", __FILE__,
        hcc1, dboff, rtsoff);
    ++controller_count;

    if (xhci_start_controller(ctrl) != 0) {
        --controller_count;
        return;
    }

    xhci_completion_t completion;
    if (xhci_submit_command(XHCI_TRB_NOOP_COMMAND, 0, 0, 0, &completion) == 0)
        done("xHCI No-Op command executed", __FILE__);
    uint8_t slot_id;
    if (xhci_enable_slot(&slot_id) == 0)
        done("xHCI Enable Slot allocated slot %u", __FILE__, slot_id);
    else
        warn("xHCI Enable Slot command failed", __FILE__);

    xhci_poll_controller(ctrl);
}
