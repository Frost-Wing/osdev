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
#define XHCI_TRB_SETUP_STAGE 2
#define XHCI_TRB_DATA_STAGE 3
#define XHCI_TRB_STATUS_STAGE 4
#define XHCI_TRB_DISABLE_SLOT 10
#define XHCI_TRB_ADDRESS_DEVICE 11
#define XHCI_TRB_CONFIGURE_ENDPOINT 12
#define XHCI_TRB_EVALUATE_CONTEXT 13
#define XHCI_TRB_RESET_ENDPOINT 14

#define XHCI_TRB_CHAIN (1U << 4)
#define XHCI_TRB_IDT (1U << 6)
#define XHCI_TRB_DIR_IN (1U << 16)

#define XHCI_PORT_CCS (1U << 0)
#define XHCI_PORT_PED (1U << 1)
#define XHCI_PORT_PR (1U << 4)
#define XHCI_PORT_PP (1U << 9)
#define XHCI_PORT_SPEED_SHIFT 10
#define XHCI_PORT_CSC (1U << 17)
#define XHCI_PORT_PRC (1U << 21)
#define XHCI_PORT_WPR (1U << 31)
#define XHCI_PORT_CHANGE_MASK (0x7FU << 17)
#define XHCI_PORT_RW_MASK ((1U << 9) | (7U << 25))

#define XHCI_EVENT_RING_SIZE 256
#define USB_REGISTRY_CAPACITY 64
#define USB_CONFIGURATION_MAX 4096
#define USB_DESCRIPTOR_DEVICE 1
#define USB_DESCRIPTOR_CONFIGURATION 2
#define USB_DESCRIPTOR_INTERFACE 4
#define USB_DESCRIPTOR_ENDPOINT 5
#define USB_REQUEST_GET_DESCRIPTOR 6
#define USB_REQUEST_SET_CONFIGURATION 9
#define USB_CLASS_HUB 9
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
    uint64_t pending_transfer_phys;
    uint16_t event_index;
    uint8_t event_cycle;
    uint8_t event_polling;
    uint8_t port_known[256];
    uint8_t port_pending[256];
    uint8_t port_slot[256];
    usb_device_t *slot_device[256];
    uint8_t command_done;
    uint8_t command_code;
    uint8_t command_slot;
    uint8_t transfer_done;
    uint8_t transfer_code;
    uint8_t transfer_slot;
    uint8_t transfer_endpoint;
    uint32_t transfer_residual;
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
static usb_device_t *usb_registry[USB_REGISTRY_CAPACITY];
static size_t usb_registry_count;

typedef struct {
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    usb_class_driver_callback_t callback;
} usb_class_driver_t;

static usb_class_driver_t usb_class_drivers[USB_MAX_CLASS_DRIVERS];
static uint8_t usb_class_driver_count;

static int xhci_enumerate_port(xhci_controller_t *ctrl, uint8_t port);
static void xhci_disconnect_port(xhci_controller_t *ctrl, uint8_t port);

static void *xhci_dma_alloc(xhci_controller_t *ctrl, size_t bytes,
    uint64_t *physical) {
    size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t page = allocate_pages_contiguous(pages);
    if (!page)
        return NULL;
    if (!ctrl->address_64 && page + pages * PAGE_SIZE - 1U > UINT32_MAX) {
        for (size_t i = 0; i < pages; ++i)
            free_page(page + i * PAGE_SIZE);
        return NULL;
    }
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
        if (slot == ctrl->transfer_slot &&
            endpoint == ctrl->transfer_endpoint) {
            ctrl->transfer_code = code;
            ctrl->transfer_residual = event->status & 0x00FFFFFFU;
            ctrl->transfer_done = 1;
        }
        // info("xHCI transfer completion: %s (%u), slot %u endpoint %u", __FILE__,
        //     xhci_completion_name(code), code, slot,
        //     endpoint);
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
    if (__atomic_test_and_set(&ctrl->event_polling, __ATOMIC_ACQUIRE))
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
    __atomic_clear(&ctrl->event_polling, __ATOMIC_RELEASE);
}

static void xhci_dma_free(uint64_t physical, size_t bytes) {
    if (!physical || !bytes)
        return;
    size_t pages = (bytes + PAGE_SIZE - 1U) / PAGE_SIZE;
    for (size_t i = 0; i < pages; ++i)
        free_page((uintptr_t)(physical + i * PAGE_SIZE));
}

static uint32_t *xhci_input_context_slot(xhci_controller_t *ctrl, void *input) {
    return (uint32_t *)((uint8_t *)input + ctrl->context_size);
}

static uint32_t *xhci_input_context_endpoint(xhci_controller_t *ctrl,
    void *input, uint8_t endpoint_id) {
    return (uint32_t *)((uint8_t *)input +
        (size_t)(endpoint_id + 1U) * ctrl->context_size);
}

static uint32_t *xhci_device_context_slot(xhci_controller_t *ctrl,
    uint64_t physical) {
    return (uint32_t *)paging_phys_to_virt((uintptr_t)physical);
}

static uint32_t *xhci_device_context_endpoint(xhci_controller_t *ctrl,
    uint64_t physical, uint8_t endpoint_id) {
    return (uint32_t *)((uint8_t *)xhci_device_context_slot(ctrl, physical) +
        (size_t)endpoint_id * ctrl->context_size);
}

static int xhci_ring_push(xhci_ring_t *ring, const xhci_trb_t *source,
    uint64_t *trb_physical) {
    uint16_t next = (uint16_t)(ring->enqueue + 1U);
    if (next == XHCI_TRB_RING_ENTRIES - 1U)
        next = 0;
    if (next == ring->dequeue)
        return -1;

    xhci_trb_t *target = &ring->trbs[ring->enqueue];
    target->parameter = source->parameter;
    target->status = source->status;
    target->control = (source->control & ~XHCI_TRB_CYCLE) | ring->cycle;
    if (trb_physical)
        *trb_physical = ring->physical +
            (uint64_t)ring->enqueue * sizeof(xhci_trb_t);
    __atomic_thread_fence(__ATOMIC_RELEASE);

    if (++ring->enqueue == XHCI_TRB_RING_ENTRIES - 1U) {
        ring->enqueue = 0;
        ring->trbs[XHCI_TRB_RING_ENTRIES - 1U].control =
            (XHCI_TRB_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC |
            ring->cycle;
        ring->cycle ^= 1U;
    }
    return 0;
}

static int xhci_wait_transfer(xhci_controller_t *ctrl, uint8_t slot_id,
    uint8_t endpoint_id) {
    for (uint32_t i = 0; i < XHCI_COMMAND_TIMEOUT; ++i) {
        xhci_poll_events(ctrl);
        if (ctrl->transfer_done) {
            uint8_t code = ctrl->transfer_code;
            ctrl->transfer_done = 0;
            if (code == 1 || code == 13)
                return 0;
            error("USB control transfer failed: %s (%u)", __FILE__,
                xhci_completion_name(code), code);
            return -1;
        }
        __asm__ volatile("pause");
    }
    error("USB control transfer timed out on slot %u", __FILE__, slot_id);
    ctrl->transfer_done = 0;
    return -1;
}

static int xhci_control_transfer(xhci_controller_t *ctrl, uint8_t slot_id,
    uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
    void *data, uint16_t length) {
    xhci_ring_t *ring = ctrl->transfer[slot_id][1];
    if (!ring || (length && !data))
        return -1;

    uint8_t setup[8] = {
        request_type, request, (uint8_t)value, (uint8_t)(value >> 8),
        (uint8_t)index, (uint8_t)(index >> 8),
        (uint8_t)length, (uint8_t)(length >> 8)
    };
    uint64_t setup_parameter = 0;
    for (uint8_t i = 0; i < sizeof(setup); ++i)
        setup_parameter |= (uint64_t)setup[i] << (i * 8U);

    xhci_trb_t trb = {0};
    trb.parameter = setup_parameter;
    trb.status = sizeof(setup);
    uint32_t transfer_type = length ?
        ((request_type & 0x80U) ? 3U : 2U) : 0U;
    trb.control = (XHCI_TRB_SETUP_STAGE << XHCI_TRB_TYPE_SHIFT) |
        XHCI_TRB_IDT | XHCI_TRB_CHAIN | (transfer_type << 16);
    if (xhci_ring_push(ring, &trb, NULL) != 0)
        return -1;

    if (length) {
        uint64_t data_physical = fast_virt_to_phys(data);
        if (!ctrl->address_64 &&
            data_physical + length - 1U > UINT32_MAX)
            return -1;
        trb.parameter = data_physical;
        trb.status = length;
        trb.control = (XHCI_TRB_DATA_STAGE << XHCI_TRB_TYPE_SHIFT) |
            XHCI_TRB_CHAIN |
            ((request_type & 0x80U) ? XHCI_TRB_DIR_IN : 0U);
        if (xhci_ring_push(ring, &trb, NULL) != 0)
            return -1;
    }

    trb.parameter = 0;
    trb.status = 0;
    trb.control = (XHCI_TRB_STATUS_STAGE << XHCI_TRB_TYPE_SHIFT) |
        XHCI_TRB_IOC |
        ((!length || !(request_type & 0x80U)) ? XHCI_TRB_DIR_IN : 0U);
    if (xhci_ring_push(ring, &trb, &ctrl->pending_transfer_phys) != 0)
        return -1;
    ctrl->transfer_slot = slot_id;
    ctrl->transfer_endpoint = 1;
    ctrl->transfer_done = 0;
    if (xhci_ring_doorbell(slot_id, 1) != 0)
        return -1;
    return xhci_wait_transfer(ctrl, slot_id, 1);
}

int usb_control_request(usb_device_t *device, uint8_t request_type,
    uint8_t request, uint16_t value, uint16_t index, void *data,
    uint16_t length) {
    if (!device || !controller_count || !controllers[0].initialized)
        return -1;
    return xhci_control_transfer(&controllers[0], device->slot_id,
        request_type, request, value, index, data, length);
}

static int xhci_issue_slot_command(uint8_t command, uint64_t input_physical,
    uint8_t slot_id) {
    xhci_completion_t completion;
    return xhci_submit_command(command, (uint32_t)input_physical,
        (uint32_t)(input_physical >> 32), (uint32_t)slot_id << 24,
        &completion);
}

static void xhci_fill_endpoint_context(uint32_t *context,
    uint8_t endpoint_type, uint16_t max_packet_size, uint64_t dequeue,
    uint16_t average_trb_length) {
    context[0] = 0;
    context[1] = (3U << 1) | ((uint32_t)endpoint_type << 3) |
        ((uint32_t)max_packet_size << 16);
    context[2] = (uint32_t)dequeue;
    context[3] = (uint32_t)(dequeue >> 32);
    context[4] = average_trb_length;
}

static int usb_class_matches(const usb_class_driver_t *driver,
    const usb_interface_t *interface) {
    return (driver->interface_class == 0xFFU ||
            driver->interface_class == interface->interface_class) &&
        (driver->interface_subclass == 0xFFU ||
            driver->interface_subclass == interface->interface_subclass) &&
        (driver->interface_protocol == 0xFFU ||
            driver->interface_protocol == interface->interface_protocol);
}

int usb_register_class_driver(uint8_t interface_class,
    uint8_t interface_subclass, uint8_t interface_protocol,
    usb_class_driver_callback_t callback) {
    if (!callback || usb_class_driver_count >= USB_MAX_CLASS_DRIVERS)
        return -1;
    usb_class_driver_t *driver = &usb_class_drivers[usb_class_driver_count++];
    driver->interface_class = interface_class;
    driver->interface_subclass = interface_subclass;
    driver->interface_protocol = interface_protocol;
    driver->callback = callback;
    for (size_t i = 0; i < USB_REGISTRY_CAPACITY; ++i) {
        usb_device_t *device = usb_registry[i];
        if (!device)
            continue;
        for (uint8_t j = 0; j < device->interface_count; ++j) {
            usb_interface_t *interface = &device->interfaces[j];
            if (interface->interface_class != USB_CLASS_HUB &&
                usb_class_matches(driver, interface))
                callback(device, interface, true);
        }
    }
    return 0;
}

size_t usb_device_count(void) {
    return usb_registry_count;
}

usb_device_t *usb_get_device(size_t index) {
    if (index >= usb_registry_count)
        return NULL;
    size_t current = 0;
    for (size_t i = 0; i < USB_REGISTRY_CAPACITY; ++i) {
        if (usb_registry[i] && current++ == index)
            return usb_registry[i];
    }
    return NULL;
}

static void usb_notify_class_drivers(usb_device_t *device, bool connected) {
    for (uint8_t i = 0; i < device->interface_count; ++i) {
        usb_interface_t *interface = &device->interfaces[i];
        if (interface->interface_class == USB_CLASS_HUB)
            continue;
        uint8_t matches = 0;
        for (uint8_t j = 0; j < usb_class_driver_count; ++j) {
            usb_class_driver_t *driver = &usb_class_drivers[j];
            if (usb_class_matches(driver, interface)) {
                driver->callback(device, interface, connected);
                ++matches;
            }
        }
        if (connected && interface->mass_storage && !matches)
            warn("No class driver registered for USB mass-storage interface %u",
                __FILE__, interface->number);
    }
}

static int usb_registry_add(usb_device_t *device) {
    for (size_t i = 0; i < USB_REGISTRY_CAPACITY; ++i) {
        if (!usb_registry[i]) {
            usb_registry[i] = device;
            ++usb_registry_count;
            return 0;
        }
    }
    return -1;
}

static void usb_registry_remove(usb_device_t *device) {
    for (size_t i = 0; i < USB_REGISTRY_CAPACITY; ++i) {
        if (usb_registry[i] == device) {
            usb_registry[i] = NULL;
            --usb_registry_count;
            return;
        }
    }
}

static void xhci_release_slot(xhci_controller_t *ctrl, uint8_t slot_id,
    uint64_t device_context_physical) {
    if (!slot_id)
        return;
    if (xhci_issue_slot_command(XHCI_TRB_DISABLE_SLOT, 0, slot_id) != 0) {
        error("Unable to disable xHCI slot %u; retaining its DMA resources",
            __FILE__, slot_id);
        return;
    }
    ctrl->dcbaa[slot_id] = 0;
    for (uint8_t endpoint = 1; endpoint < 32; ++endpoint) {
        xhci_ring_t *ring = ctrl->transfer[slot_id][endpoint];
        if (!ring)
            continue;
        xhci_dma_free(ring->physical, PAGE_SIZE);
        kfree(ring);
        ctrl->transfer[slot_id][endpoint] = NULL;
    }
    if (device_context_physical)
        xhci_dma_free(device_context_physical, PAGE_SIZE);
}

static int usb_parse_configuration(usb_device_t *device) {
    uint8_t *bytes = device->configuration_descriptor;
    uint16_t total = device->configuration_length;
    int current_interface = -1;
    for (uint16_t offset = 0; offset < total;) {
        if (total - offset < 2)
            return -1;
        uint8_t length = bytes[offset];
        uint8_t type = bytes[offset + 1];
        if (length < 2 || length > total - offset)
            return -1;

        if (type == USB_DESCRIPTOR_INTERFACE) {
            if (length < 9)
                return -1;
            current_interface = -1;
            if (bytes[offset + 3] == 0) {
                if (device->interface_count >= USB_MAX_INTERFACES)
                    return -1;
                usb_interface_t *interface =
                    &device->interfaces[device->interface_count++];
                interface->number = bytes[offset + 2];
                interface->alternate_setting = bytes[offset + 3];
                interface->endpoint_count = bytes[offset + 4];
                interface->interface_class = bytes[offset + 5];
                interface->interface_subclass = bytes[offset + 6];
                interface->interface_protocol = bytes[offset + 7];
                interface->string_index = bytes[offset + 8];
                interface->mass_storage =
                    interface->interface_class == 0x08U &&
                    interface->interface_subclass == 0x06U &&
                    interface->interface_protocol == 0x50U;
                current_interface = device->interface_count - 1;
                info("USB interface %u: class 0x%02x subclass 0x%02x protocol 0x%02x endpoints %u%s",
                    __FILE__, interface->number, interface->interface_class,
                    interface->interface_subclass, interface->interface_protocol,
                    interface->endpoint_count,
                    interface->mass_storage ? " (SCSI bulk-only mass storage)" : "");
            }
        } else if (type == USB_DESCRIPTOR_ENDPOINT) {
            if (length < 7)
                return -1;
            if (current_interface < 0) {
                offset += length;
                continue;
            }
            if (device->endpoint_count >= USB_MAX_ENDPOINTS)
                return -1;
            usb_endpoint_t *endpoint =
                &device->endpoints[device->endpoint_count++];
            endpoint->address = bytes[offset + 2];
            endpoint->attributes = bytes[offset + 3];
            endpoint->max_packet_size =
                (uint16_t)(bytes[offset + 4] | (bytes[offset + 5] << 8)) & 0x7FFU;
            endpoint->interval = bytes[offset + 6];
            endpoint->interface_number =
                device->interfaces[current_interface].number;
            uint8_t number = endpoint->address & 0x0FU;
            endpoint->endpoint_id = (uint8_t)(number * 2U +
                ((endpoint->address & 0x80U) ? 1U : 0U));
            info("USB endpoint 0x%02x: type %u max packet %u interval %u",
                __FILE__, endpoint->address, endpoint->attributes & 3U,
                endpoint->max_packet_size, endpoint->interval);
        }
        offset += length;
    }
    return device->interface_count ? 0 : -1;
}

static void xhci_free_unregistered_device(usb_device_t *device) {
    if (!device)
        return;
    if (device->configuration_descriptor)
        kfree(device->configuration_descriptor);
    kfree(device);
}

static int xhci_configure_bulk_endpoints(xhci_controller_t *ctrl,
    usb_device_t *device) {
    uint64_t input_physical;
    void *input = xhci_dma_alloc(ctrl, PAGE_SIZE, &input_physical);
    if (!input)
        return -1;
    uint32_t *input_control = input;
    uint32_t *input_slot = xhci_input_context_slot(ctrl, input);
    uint32_t *device_slot = xhci_device_context_slot(ctrl,
        device->device_context_physical);
    memcpy(input_slot, device_slot, ctrl->context_size);
    input_control[1] = 1U;

    uint8_t max_context = 1;
    uint8_t configured = 0;
    int result = 0;
    for (uint8_t i = 0; i < device->endpoint_count; ++i) {
        usb_endpoint_t *endpoint = &device->endpoints[i];
        if ((endpoint->attributes & 3U) != 2U)
            continue;
        if (!endpoint->endpoint_id || endpoint->endpoint_id > 31 ||
            !endpoint->max_packet_size) {
            warn("Ignoring invalid USB bulk endpoint 0x%02x", __FILE__,
                endpoint->address);
            continue;
        }
        uint64_t dequeue;
        if (xhci_create_transfer_ring(device->slot_id, endpoint->endpoint_id,
                &dequeue) != 0) {
            result = -1;
            break;
        }
        uint8_t endpoint_type = (endpoint->address & 0x80U) ? 6U : 2U;
        uint32_t *context = xhci_input_context_endpoint(ctrl, input,
            endpoint->endpoint_id);
        xhci_fill_endpoint_context(context, endpoint_type,
            endpoint->max_packet_size, dequeue, endpoint->max_packet_size);
        input_control[1] |= 1U << endpoint->endpoint_id;
        if (endpoint->endpoint_id > max_context)
            max_context = endpoint->endpoint_id;
        ++configured;
    }
    if (result == 0 && configured) {
        input_slot[0] = (input_slot[0] & ~(0x1FU << 27)) |
            ((uint32_t)max_context << 27);
        result = xhci_issue_slot_command(XHCI_TRB_CONFIGURE_ENDPOINT,
            input_physical, device->slot_id);
        if (result == 0)
            done("Configured %u USB bulk endpoint(s) on slot %u", __FILE__,
                configured, device->slot_id);
    }
    xhci_dma_free(input_physical, PAGE_SIZE);
    return result;
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
            if (status & XHCI_PORT_CCS) {
                info("xHCI port %u connected (speed ID %u)", __FILE__, port,
                    (status >> 10) & 0xFU);
                ctrl->port_pending[port] =
                    ctrl->port_slot[port] ? 3U : 1U;
            } else if (status & XHCI_PORT_CSC) {
                info("xHCI port %u disconnected", __FILE__, port);
                ctrl->port_pending[port] = 2;
            }
        }
        if (status & (XHCI_PORT_PRC | (1U << 19)))
            info("xHCI port %u reset completed; enabled=%u", __FILE__, port,
                !!(status & XHCI_PORT_PED));

        *portsc = (status & XHCI_PORT_RW_MASK) | (status & XHCI_PORT_CHANGE_MASK);
    }
}

static void xhci_process_port_work(xhci_controller_t *ctrl) {
    if (ctrl != &controllers[0])
        return;
    for (uint16_t port = 1; port <= ctrl->max_ports; ++port) {
        uint8_t pending = ctrl->port_pending[port];
        ctrl->port_pending[port] = 0;
        if (pending == 1 || pending == 3) {
            if (pending == 3)
                xhci_disconnect_port(ctrl, (uint8_t)port);
            if (!ctrl->port_slot[port])
                (void)xhci_enumerate_port(ctrl, (uint8_t)port);
        } else if (pending == 2) {
            xhci_disconnect_port(ctrl, (uint8_t)port);
        }
    }
}

static int xhci_enumerate_port(xhci_controller_t *ctrl, uint8_t port) {
    LOG_SCOPE();
    volatile uint32_t *portsc =
        &ctrl->op[(XHCI_PORTS / 4) + ((port - 1U) * 4U)];
    uint32_t status = *portsc;
    if (!(status & XHCI_PORT_CCS))
        return -1;

    uint8_t port_speed = (uint8_t)((status >> XHCI_PORT_SPEED_SHIFT) & 0xFU);
    uint32_t reset = port_speed >= 4U ? XHCI_PORT_WPR : XHCI_PORT_PR;
    info("Resetting xHCI root port %u (speed ID %u)", __FILE__, port,
        port_speed);
    *portsc = (status & XHCI_PORT_RW_MASK) | reset;
    if (xhci_wait_bits(portsc, XHCI_PORT_PR | XHCI_PORT_WPR | XHCI_PORT_PED,
            XHCI_PORT_PED, 1000) != 0) {
        error("xHCI port %u reset did not enable the port", __FILE__, port);
        return -1;
    }
    status = *portsc;
    if (!(status & XHCI_PORT_CCS)) {
        warn("USB device disconnected while resetting port %u", __FILE__, port);
        return -1;
    }
    uint8_t speed = (uint8_t)((status >> XHCI_PORT_SPEED_SHIFT) & 0xFU);
    if (speed < 1U || speed > 5U) {
        error("Unsupported USB speed ID %u on port %u", __FILE__, speed, port);
        return -1;
    }
    info("USB root port %u reset complete; speed ID %u", __FILE__, port,
        speed);

    uint8_t slot_id = 0;
    if (xhci_enable_slot(&slot_id) != 0) {
        error("Unable to enable xHCI slot for port %u", __FILE__, port);
        return -1;
    }

    usb_device_t *device = kmalloc(sizeof(*device));
    uint64_t device_context_physical = 0;
    uint64_t input_physical = 0;
    void *input = NULL;
    uint8_t *descriptor_buffer = NULL;
    if (!device) {
        error("Unable to allocate USB device record", __FILE__);
        xhci_release_slot(ctrl, slot_id, 0);
        return -1;
    }
    memset(device, 0, sizeof(*device));
    device->slot_id = slot_id;
    device->root_port = port;
    device->speed = speed;

    void *device_context = xhci_dma_alloc(ctrl, PAGE_SIZE,
        &device_context_physical);
    if (!device_context) {
        error("Unable to allocate USB device context", __FILE__);
        goto failed;
    }
    ctrl->dcbaa[slot_id] = device_context_physical;
    device->device_context_physical = device_context_physical;

    uint64_t ep0_dequeue;
    if (xhci_create_transfer_ring(slot_id, 1, &ep0_dequeue) != 0) {
        error("Unable to allocate EP0 transfer ring for slot %u", __FILE__,
            slot_id);
        goto failed;
    }

    input = xhci_dma_alloc(ctrl, PAGE_SIZE, &input_physical);
    if (!input) {
        error("Unable to allocate USB input context", __FILE__);
        goto failed;
    }
    uint32_t *input_control = input;
    uint32_t *slot_context = xhci_input_context_slot(ctrl, input);
    uint32_t *ep0_context = xhci_input_context_endpoint(ctrl, input, 1);
    input_control[1] = 3U;
    slot_context[0] = ((uint32_t)speed << 20) | (1U << 27);
    slot_context[1] = (uint32_t)port << 16;

    uint16_t initial_packet = speed == 2U ? 8U :
        (speed == 3U ? 64U : (speed >= 4U ? 512U : 8U));
    xhci_fill_endpoint_context(ep0_context, 4U, initial_packet,
        ep0_dequeue, 8U);
    if (xhci_issue_slot_command(XHCI_TRB_ADDRESS_DEVICE, input_physical,
            slot_id) != 0) {
        error("Address Device failed for USB slot %u", __FILE__, slot_id);
        goto failed;
    }
    xhci_dma_free(input_physical, PAGE_SIZE);
    input = NULL;
    input_physical = 0;
    done("USB slot %u addressed on root port %u", __FILE__, slot_id, port);

    descriptor_buffer = kmalloc_aligned(64, PAGE_SIZE);
    if (!descriptor_buffer) {
        error("Unable to allocate USB descriptor buffer", __FILE__);
        goto failed;
    }
    memset(descriptor_buffer, 0, 64);
    if (xhci_control_transfer(ctrl, slot_id, 0x80,
            USB_REQUEST_GET_DESCRIPTOR, USB_DESCRIPTOR_DEVICE << 8, 0,
            descriptor_buffer, 8) != 0) {
        error("Unable to read initial USB device descriptor", __FILE__);
        goto failed;
    }
    if (descriptor_buffer[0] < 8U ||
        descriptor_buffer[1] != USB_DESCRIPTOR_DEVICE) {
        error("Malformed initial USB device descriptor", __FILE__);
        goto failed;
    }
    uint16_t packet_size = descriptor_buffer[7];
    if (speed >= 4U) {
        if (packet_size != 9U) {
            error("Invalid SuperSpeed EP0 packet-size exponent %u", __FILE__,
                packet_size);
            goto failed;
        }
        packet_size = 512U;
    }
    if ((speed == 2U && packet_size != 8U) ||
        (speed == 3U && packet_size != 64U) ||
        (speed == 1U && packet_size != 8U && packet_size != 16U &&
            packet_size != 32U && packet_size != 64U)) {
        error("Invalid EP0 max packet size %u for USB speed ID %u", __FILE__,
            packet_size, speed);
        goto failed;
    }
    if (packet_size != initial_packet) {
        input = xhci_dma_alloc(ctrl, PAGE_SIZE, &input_physical);
        if (!input) {
            error("Unable to allocate EP0 update context", __FILE__);
            goto failed;
        }
        input_control = input;
        input_control[1] = 1U << 1;
        ep0_context = xhci_input_context_endpoint(ctrl, input, 1);
        memcpy(ep0_context,
            xhci_device_context_endpoint(ctrl, device_context_physical, 1),
            ctrl->context_size);
        ep0_context[1] = (ep0_context[1] & 0xFFFFU) |
            ((uint32_t)packet_size << 16);
        if (xhci_issue_slot_command(XHCI_TRB_EVALUATE_CONTEXT,
                input_physical, slot_id) != 0) {
            error("Unable to update EP0 max packet size on slot %u", __FILE__,
                slot_id);
            goto failed;
        }
        xhci_dma_free(input_physical, PAGE_SIZE);
        input = NULL;
        input_physical = 0;
        done("USB slot %u EP0 max packet size set to %u", __FILE__, slot_id,
            packet_size);
    }

    memset(descriptor_buffer, 0, 64);
    if (xhci_control_transfer(ctrl, slot_id, 0x80,
            USB_REQUEST_GET_DESCRIPTOR, USB_DESCRIPTOR_DEVICE << 8, 0,
            descriptor_buffer, sizeof(usb_device_descriptor_t)) != 0) {
        error("Unable to read full USB device descriptor", __FILE__);
        goto failed;
    }
    memcpy(&device->descriptor, descriptor_buffer,
        sizeof(device->descriptor));
    if (device->descriptor.length < sizeof(device->descriptor) ||
        device->descriptor.descriptor_type != USB_DESCRIPTOR_DEVICE) {
        error("USB device returned a malformed device descriptor", __FILE__);
        goto failed;
    }
    info("USB device: VID:PID %04x:%04x, USB %x.%02x, class 0x%02x, packet %u",
        __FILE__, device->descriptor.vendor_id, device->descriptor.product_id,
        device->descriptor.usb_version >> 8,
        device->descriptor.usb_version & 0xFFU,
        device->descriptor.device_class, packet_size);
    done("Read USB device descriptor (%u bytes)", __FILE__,
        device->descriptor.length);
    if (device->descriptor.device_class == USB_CLASS_HUB) {
        warn("USB hub on root port %u ignored; hub enumeration is not supported",
            __FILE__, port);
        goto failed;
    }
    if (device->descriptor.configuration_count == 0) {
        error("USB device has no configurations", __FILE__);
        goto failed;
    }

    memset(descriptor_buffer, 0, 64);
    if (xhci_control_transfer(ctrl, slot_id, 0x80,
            USB_REQUEST_GET_DESCRIPTOR,
            (USB_DESCRIPTOR_CONFIGURATION << 8), 0,
            descriptor_buffer, 9) != 0 ||
        descriptor_buffer[0] < 9 ||
        descriptor_buffer[1] != USB_DESCRIPTOR_CONFIGURATION) {
        error("Unable to read USB configuration descriptor header", __FILE__);
        goto failed;
    }
    uint16_t configuration_length =
        (uint16_t)(descriptor_buffer[2] | (descriptor_buffer[3] << 8));
    if (configuration_length < 9U ||
        configuration_length > USB_CONFIGURATION_MAX) {
        error("Unsupported USB configuration descriptor length %u", __FILE__,
            configuration_length);
        goto failed;
    }
    device->configuration_descriptor = kmalloc_aligned(configuration_length,
        PAGE_SIZE);
    if (!device->configuration_descriptor) {
        error("Unable to allocate USB configuration descriptor buffer",
            __FILE__);
        goto failed;
    }
    device->configuration_length = configuration_length;
    if (xhci_control_transfer(ctrl, slot_id, 0x80,
            USB_REQUEST_GET_DESCRIPTOR,
            (USB_DESCRIPTOR_CONFIGURATION << 8), 0,
            device->configuration_descriptor, configuration_length) != 0) {
        error("Unable to read full USB configuration descriptor", __FILE__);
        goto failed;
    }
    info("USB configuration descriptor: %u bytes, %u interfaces, value %u",
        __FILE__, configuration_length,
        device->configuration_descriptor[4],
        device->configuration_descriptor[5]);
    done("Read USB configuration descriptor", __FILE__);
    if (usb_parse_configuration(device) != 0) {
        error("Malformed or unsupported USB interface/endpoint descriptors",
            __FILE__);
        goto failed;
    }
    for (uint8_t i = 0; i < device->interface_count; ++i) {
        if (device->interfaces[i].interface_class == USB_CLASS_HUB)
            warn("USB hub interface %u ignored", __FILE__,
                device->interfaces[i].number);
    }

    device->configuration_value = device->configuration_descriptor[5];
    if (xhci_control_transfer(ctrl, slot_id, 0x00,
            USB_REQUEST_SET_CONFIGURATION, device->configuration_value, 0,
            NULL, 0) != 0) {
        error("SET_CONFIGURATION failed for USB slot %u", __FILE__, slot_id);
        goto failed;
    }
    done("USB slot %u set to configuration %u", __FILE__, slot_id,
        device->configuration_value);
    if (xhci_configure_bulk_endpoints(ctrl, device) != 0) {
        error("Unable to configure USB bulk endpoints on slot %u", __FILE__,
            slot_id);
        goto failed;
    }
    if (usb_registry_add(device) != 0) {
        error("USB device registry is full", __FILE__);
        goto failed;
    }
    ctrl->slot_device[slot_id] = device;
    ctrl->port_slot[port] = slot_id;
    info("USB device registered in slot %u (%u interfaces, %u endpoints)",
        __FILE__, slot_id, device->interface_count, device->endpoint_count);
    usb_notify_class_drivers(device, true);
    done("USB enumeration complete on root port %u", __FILE__, port);
    kfree(descriptor_buffer);
    return 0;

failed:
    if (input)
        xhci_dma_free(input_physical, PAGE_SIZE);
    if (descriptor_buffer)
        kfree(descriptor_buffer);
    if (device->configuration_descriptor)
        kfree(device->configuration_descriptor);
    xhci_release_slot(ctrl, slot_id, device_context_physical);
    kfree(device);
    return -1;
}

static void xhci_disconnect_port(xhci_controller_t *ctrl, uint8_t port) {
    uint8_t slot_id = ctrl->port_slot[port];
    if (!slot_id)
        return;
    usb_device_t *device = ctrl->slot_device[slot_id];
    info("USB device disconnected from root port %u (slot %u)", __FILE__,
        port, slot_id);
    if (device) {
        usb_notify_class_drivers(device, false);
        usb_registry_remove(device);
    }
    ctrl->slot_device[slot_id] = NULL;
    ctrl->port_slot[port] = 0;
    xhci_release_slot(ctrl, slot_id,
        device ? device->device_context_physical : ctrl->dcbaa[slot_id]);
    xhci_free_unregistered_device(device);
    done("USB resources released for root port %u", __FILE__, port);
}

static void xhci_poll_controller(xhci_controller_t *ctrl) {
    if (!ctrl->initialized)
        return;
    xhci_poll_events(ctrl);
    xhci_poll_ports(ctrl);
    xhci_process_port_work(ctrl);
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

int xhci_bulk_transfer(uint8_t slot_id, uint8_t endpoint_id, void *buffer,
    uint32_t length, uint32_t *actual) {
    if (!controller_count || !controllers[0].initialized || slot_id == 0 ||
        slot_id > controllers[0].max_slots || endpoint_id == 0 ||
        endpoint_id > 31 || !buffer || length == 0 || length > 0x1FFFFU)
        return -1;

    xhci_controller_t *ctrl = &controllers[0];
    xhci_ring_t *ring = ctrl->transfer[slot_id][endpoint_id];
    if (!ring)
        return -1;

    uint64_t buffer_phys = fast_virt_to_phys(buffer);
    if (buffer_phys > UINT64_MAX - (uint64_t)(length - 1U) ||
        (!ctrl->address_64 && buffer_phys + length - 1U > UINT32_MAX))
        return -1;

    xhci_trb_t *trb = &ring->trbs[ring->enqueue];
    uint16_t next = (uint16_t)(ring->enqueue + 1U);
    if (next == XHCI_TRB_RING_ENTRIES - 1U)
        next = 0;
    if (next == ring->dequeue)
        return -1;

    trb->parameter = buffer_phys;
    trb->status = length & 0x1FFFFU;
    trb->control = (XHCI_TRB_NORMAL << XHCI_TRB_TYPE_SHIFT) |
        XHCI_TRB_IOC | ring->cycle;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    if (++ring->enqueue == XHCI_TRB_RING_ENTRIES - 1U) {
        ring->enqueue = 0;
        ring->trbs[XHCI_TRB_RING_ENTRIES - 1U].control =
            (XHCI_TRB_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_TC |
            ring->cycle;
        ring->cycle ^= 1U;
    }

    ctrl->transfer_slot = slot_id;
    ctrl->transfer_endpoint = endpoint_id;
    ctrl->transfer_done = 0;
    ctrl->transfer_residual = length;
    if (xhci_ring_doorbell(slot_id, endpoint_id) != 0)
        return -1;
    if (xhci_wait_transfer(ctrl, slot_id, endpoint_id) != 0)
        return -1;

    if (ctrl->transfer_residual > length)
        return -1;
    if (actual)
        *actual = length - ctrl->transfer_residual;
    return 0;
}

int xhci_reset_endpoint(uint8_t slot_id, uint8_t endpoint_id) {
    if (!controller_count || !controllers[0].initialized || slot_id == 0 ||
        slot_id > controllers[0].max_slots || endpoint_id == 0 ||
        endpoint_id > 31)
        return -1;

    uint32_t control = ((uint32_t)slot_id << 24) |
        ((uint32_t)endpoint_id << 16);
    if (xhci_submit_command(XHCI_TRB_RESET_ENDPOINT, 0, 0, control, NULL) != 0)
        return -1;

    xhci_ring_t *ring = controllers[0].transfer[slot_id][endpoint_id];
    if (ring)
        ring->dequeue = ring->enqueue;
    return 0;
}

void xhci_interrupt_handler(InterruptFrame *frame) {
    (void)frame;
    for (uint8_t i = 0; i < controller_count; ++i)
        xhci_poll_events(&controllers[i]);
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
    xhci_poll_controller(ctrl);
}
