/**
 * @file ehci.c
 * @brief EHCI USB 2.0 host controller and device support.
 */
#include <cc-asm.h>
#include <ehci.h>
#include <graphics.h>
#include <heap.h>
#include <memory.h>
#include <paging.h>
#include <pci.h>
#include <pit.h>
#include <strings.h>

#define EHCI_MAX_CONTROLLERS 4
#define EHCI_MAX_DEVICES 32
#define EHCI_CAP_LENGTH 0x00
#define EHCI_HCSPARAMS 0x04
#define EHCI_HCCPARAMS 0x08
#define EHCI_USBCMD 0x00
#define EHCI_USBSTS 0x04
#define EHCI_USBINTR 0x08
#define EHCI_CTRLDSSEGMENT 0x10
#define EHCI_ASYNCLISTADDR 0x18
#define EHCI_CONFIGFLAG 0x40
#define EHCI_PORTSC 0x44
#define EHCI_CMD_RUN (1U << 0)
#define EHCI_CMD_RESET (1U << 1)
#define EHCI_CMD_ASYNC_ENABLE (1U << 5)
#define EHCI_STS_HALTED (1U << 12)
#define EHCI_STS_ASYNC_STATUS (1U << 15)
#define EHCI_PORT_CONNECT (1U << 0)
#define EHCI_PORT_ENABLE (1U << 2)
#define EHCI_PORT_RESET (1U << 8)
#define EHCI_PORT_POWER (1U << 12)
#define EHCI_PORT_OWNER (1U << 13)
#define EHCI_PORT_CHANGE ((1U << 1) | (1U << 3) | (1U << 5))
#define EHCI_QH_LINK_TYPE (1U << 1)
#define EHCI_QH_HEAD (1U << 15)
#define EHCI_QTD_TERMINATE 1U
#define EHCI_QTD_ACTIVE (1U << 7)
#define EHCI_QTD_IOC (1U << 15)
#define EHCI_QTD_PID_OUT (0U << 8)
#define EHCI_QTD_PID_IN (1U << 8)
#define EHCI_QTD_PID_SETUP (2U << 8)
#define EHCI_QTD_CERR3 (3U << 10)
#define EHCI_QTD_TOGGLE (1U << 31)
#define EHCI_CONTROL_BUFFER_MAX 0x4000U
#define EHCI_DESCRIPTOR_CONFIGURATION 2U
#define EHCI_DESCRIPTOR_INTERFACE 4U
#define EHCI_DESCRIPTOR_ENDPOINT 5U
#define EHCI_REQUEST_GET_DESCRIPTOR 6U
#define EHCI_REQUEST_SET_ADDRESS 5U
#define EHCI_REQUEST_SET_CONFIGURATION 9U
#define EHCI_REQUEST_GET_STATUS 0U
#define EHCI_REQUEST_CLEAR_FEATURE 1U
#define EHCI_REQUEST_SET_FEATURE 3U
#define EHCI_DESCRIPTOR_HUB 0x29U
#define EHCI_HUB_CLASS 9U
#define EHCI_HUB_PORT_POWER 8U
#define EHCI_HUB_PORT_RESET 4U
#define EHCI_HUB_C_PORT_CONNECTION 16U
#define EHCI_HUB_C_PORT_ENABLE 17U
#define EHCI_HUB_C_PORT_RESET 20U
#define EHCI_HUB_PORT_CONNECTION (1U << 0)
#define EHCI_HUB_PORT_ENABLE (1U << 1)
#define EHCI_HUB_PORT_RESETTING (1U << 4)
#define EHCI_HUB_PORT_LOW_SPEED (1U << 9)
#define EHCI_HUB_PORT_HIGH_SPEED (1U << 10)
#define EHCI_HUB_MAX_PORTS 15U

typedef struct __attribute__((packed, aligned(32))) {
    uint32_t horizontal;
    uint32_t endpoint_characteristics;
    uint32_t endpoint_capabilities;
    uint32_t current_qtd;
    uint32_t next_qtd;
    uint32_t alternate_qtd;
    uint32_t token;
    uint32_t buffers[5];
    uint32_t extended_buffers[5];
} ehci_qh_t;

typedef struct __attribute__((packed, aligned(32))) {
    uint32_t next;
    uint32_t alternate;
    uint32_t token;
    uint32_t buffers[5];
    uint32_t extended_buffers[5];
} ehci_qtd_t;

typedef struct {
    volatile uint32_t *cap;
    volatile uint32_t *op;
    ehci_qh_t *async_head;
    ehci_qh_t *transfer_qh;
    uint64_t async_head_phys;
    uint64_t transfer_qh_phys;
    uint64_t bar_phys;
    uint32_t port_count;
    uint8_t address_next;
    uint8_t port_power_control;
    uint8_t port_connected[16];
    uint8_t port_enumerated[16];
    uint8_t port_enumeration_attempts[16];
    uint64_t port_retry_tick[16];
    uint8_t initialized;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    volatile int lock;
    volatile int poll_lock;
} ehci_controller_t;

typedef struct ehci_device {
    ehci_controller_t *controller;
    usb_device_t *device;
    struct ehci_device *parent_hub;
    uint8_t parent_port;
    uint8_t address;
    uint8_t port;
    uint8_t ep0_packet;
    uint8_t active;
    uint8_t speed;
    uint8_t hub;
    uint8_t hub_port_count;
    uint8_t hub_port_connected[EHCI_HUB_MAX_PORTS];
    uint8_t hub_port_enumerated[EHCI_HUB_MAX_PORTS];
    uint8_t hub_port_attempts[EHCI_HUB_MAX_PORTS];
    uint64_t hub_port_retry_tick[EHCI_HUB_MAX_PORTS];
    uint64_t hub_poll_tick;
} ehci_device_t;

static ehci_controller_t controllers[EHCI_MAX_CONTROLLERS];
static uint8_t controller_count;
static ehci_device_t ehci_devices[EHCI_MAX_DEVICES];

static void *ehci_dma_alloc(size_t bytes, uint64_t *physical) {
    size_t pages = (bytes + PAGE_SIZE - 1U) / PAGE_SIZE;
    uintptr_t page = allocate_pages_contiguous(pages);
    if (!page)
        return 0;
    if (page + pages * PAGE_SIZE - 1U > UINT32_MAX) {
        for (size_t i = 0; i < pages; ++i)
            free_page(page + i * PAGE_SIZE);
        return 0;
    }
    *physical = page;
    return paging_phys_to_virt(page);
}

static void ehci_dma_free(uint64_t physical, size_t bytes) {
    if (!physical || !bytes)
        return;
    size_t pages = (bytes + PAGE_SIZE - 1U) / PAGE_SIZE;
    for (size_t i = 0; i < pages; ++i)
        free_page((uintptr_t)(physical + i * PAGE_SIZE));
}

static int ehci_wait(volatile uint32_t *reg, uint32_t mask, uint32_t value,
    uint32_t iterations) {
    while (iterations--) {
        if ((*reg & mask) == value)
            return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

static ehci_device_t *ehci_find_device(const usb_device_t *device) {
    for (size_t i = 0; i < EHCI_MAX_DEVICES; ++i) {
        if (ehci_devices[i].active && ehci_devices[i].device == device)
            return &ehci_devices[i];
    }
    return NULL;
}

static uint8_t ehci_allocate_address(ehci_controller_t *ctrl) {
    for (uint16_t attempt = 0; attempt < 127U; ++attempt) {
        uint8_t candidate = (uint8_t)(ctrl->address_next + 1U);
        if (!candidate || candidate > 127U)
            candidate = 1U;
        ctrl->address_next = candidate;

        uint8_t in_use = 0;
        for (size_t i = 0; i < EHCI_MAX_DEVICES; ++i) {
            if (ehci_devices[i].active &&
                ehci_devices[i].controller == ctrl &&
                ehci_devices[i].address == candidate) {
                in_use = 1;
                break;
            }
        }
        if (!in_use)
            return candidate;
    }
    return 0;
}

static void ehci_set_qh(ehci_controller_t *ctrl, ehci_device_t *device,
    uint8_t endpoint, uint16_t packet_size, uint8_t toggle,
    uint8_t control_endpoint) {
    ehci_qh_t *qh = ctrl->transfer_qh;
    uint32_t ep_num = endpoint & 0x0FU;
    qh->endpoint_characteristics = device->address |
                                   (ep_num << 8) |
                                   ((uint32_t)device->speed << 12) |
                                   ((uint32_t)packet_size << 16) |
                                   0;
    if (control_endpoint)
        qh->endpoint_characteristics |= 1U << 14;
    if (control_endpoint && device->speed != 2U)
        qh->endpoint_characteristics |= 1U << 27;
    qh->endpoint_capabilities = (1U << 30);
    if (device->parent_hub && device->speed != 2U) {
        qh->endpoint_capabilities |=
            ((uint32_t)device->parent_hub->address << 16) |
            ((uint32_t)device->parent_port << 23);
    }
    qh->current_qtd = 0;
    qh->next_qtd = EHCI_QTD_TERMINATE;
    qh->alternate_qtd = EHCI_QTD_TERMINATE;
    qh->token = toggle ? EHCI_QTD_TOGGLE : 0;
}

static void ehci_qtd_buffer(ehci_qtd_t *qtd, uint64_t physical,
    uint32_t length) {
    memset(qtd->buffers, 0, sizeof(qtd->buffers));
    memset(qtd->extended_buffers, 0, sizeof(qtd->extended_buffers));
    qtd->buffers[0] = (uint32_t)physical;
    uint64_t page = physical & ~((uint64_t)PAGE_SIZE - 1U);
    uint8_t page_count = (uint8_t)((((physical & (PAGE_SIZE - 1U)) +
                                        length) +
                                       PAGE_SIZE - 1U) /
                                   PAGE_SIZE);
    for (uint8_t i = 1; i < page_count && i < 5; ++i) {
        page += PAGE_SIZE;
        if (page <= UINT32_MAX)
            qtd->buffers[i] = (uint32_t)page;
    }
    (void)length;
}

static void ehci_qtd_prepare(ehci_qtd_t *qtd, uint64_t physical,
    uint32_t length, uint32_t pid, uint8_t toggle, uint8_t ioc) {
    qtd->next = EHCI_QTD_TERMINATE;
    qtd->alternate = EHCI_QTD_TERMINATE;
    qtd->token = EHCI_QTD_ACTIVE | EHCI_QTD_CERR3 | pid |
                 ((length & 0x7FFFU) << 16) | (toggle ? EHCI_QTD_TOGGLE : 0U) |
                 (ioc ? EHCI_QTD_IOC : 0U);
    if (length)
        ehci_qtd_buffer(qtd, physical, length);
    else {
        memset(qtd->buffers, 0, sizeof(qtd->buffers));
        memset(qtd->extended_buffers, 0, sizeof(qtd->extended_buffers));
    }
}

static int ehci_schedule(ehci_controller_t *ctrl, ehci_qtd_t *first,
    ehci_qtd_t *last) {
    volatile uint32_t *cmd = &ctrl->op[EHCI_USBCMD / 4];
    volatile uint32_t *status = &ctrl->op[EHCI_USBSTS / 4];
    *cmd &= ~EHCI_CMD_ASYNC_ENABLE;
    if (ehci_wait(status, EHCI_STS_ASYNC_STATUS, 0, 10000000U) != 0)
        return -1;

    ctrl->transfer_qh->current_qtd = 0;
    ctrl->transfer_qh->next_qtd = (uint32_t)fast_virt_to_phys(first);
    ctrl->transfer_qh->alternate_qtd = EHCI_QTD_TERMINATE;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    *cmd |= EHCI_CMD_ASYNC_ENABLE;
    if (ehci_wait(status, EHCI_STS_ASYNC_STATUS, EHCI_STS_ASYNC_STATUS,
            10000000U) != 0) {
        *cmd &= ~EHCI_CMD_ASYNC_ENABLE;
        return -1;
    }

    uint32_t iterations = 50000000U;
    while (iterations-- && (last->token & EHCI_QTD_ACTIVE)) {
        if (first->token & 0x7CU)
            break;
        __asm__ volatile("pause");
    }
    int result = (last->token & EHCI_QTD_ACTIVE) ? -1 : 0;
    for (ehci_qtd_t *qtd = first;;) {
        if (qtd->token & 0x7CU)
            result = -1;
        if (qtd == last)
            break;
        uint64_t next = ((uint64_t)qtd->next) & ~0x1FU;
        qtd = paging_phys_to_virt((uintptr_t)next);
    }
    *cmd &= ~EHCI_CMD_ASYNC_ENABLE;
    if (ehci_wait(status, EHCI_STS_ASYNC_STATUS, 0, 10000000U) != 0)
        return -1;
    return result;
}

static int ehci_transfer(ehci_device_t *device, uint8_t endpoint_address,
    uint16_t packet_size, uint8_t toggle, ehci_qtd_t *qtds,
    uint8_t qtd_count, uint8_t control_endpoint) {
    ehci_controller_t *ctrl = device->controller;
    while (__sync_lock_test_and_set(&ctrl->lock, 1))
        __asm__ volatile("pause");
    volatile uint32_t *cmd = &ctrl->op[EHCI_USBCMD / 4];
    volatile uint32_t *status = &ctrl->op[EHCI_USBSTS / 4];
    *cmd &= ~EHCI_CMD_ASYNC_ENABLE;
    if (ehci_wait(status, EHCI_STS_ASYNC_STATUS, 0, 10000000U) != 0) {
        __sync_lock_release(&ctrl->lock);
        return -1;
    }
    ehci_set_qh(ctrl, device, endpoint_address & 0x0FU, packet_size, toggle,
        control_endpoint);
    for (uint8_t i = 0; i + 1U < qtd_count; ++i)
        qtds[i].next = (uint32_t)fast_virt_to_phys(&qtds[i + 1U]);
    int result = ehci_schedule(ctrl, qtds, &qtds[qtd_count - 1U]);
    __sync_lock_release(&ctrl->lock);
    return result;
}

static int ehci_control(ehci_device_t *device, uint8_t request_type,
    uint8_t request, uint16_t value, uint16_t index, void *data,
    uint16_t length) {
    if (length > EHCI_CONTROL_BUFFER_MAX || (length && !data))
        return -1;
    uint64_t qtd_phys = 0, data_phys = 0;
    ehci_qtd_t *qtds = ehci_dma_alloc(PAGE_SIZE, &qtd_phys);
    uint8_t *bounce = NULL;
    size_t bounce_pages = 0;
    uint64_t bounce_phys = 0;
    if (!qtds)
        goto fail;
    if (length) {
        bounce_pages = (length + PAGE_SIZE - 1U) / PAGE_SIZE;
        bounce_phys = allocate_pages_contiguous(bounce_pages);
        if (!bounce_phys || bounce_phys + bounce_pages * PAGE_SIZE - 1U >
                                UINT32_MAX)
            goto fail;
        data_phys = bounce_phys;
        bounce = paging_phys_to_virt((uintptr_t)bounce_phys);
        if (!(request_type & 0x80U))
            memcpy(bounce, data, length);
    }
    uint8_t setup[8] = {
        request_type, request, (uint8_t)value, (uint8_t)(value >> 8),
        (uint8_t)index, (uint8_t)(index >> 8),
        (uint8_t)length, (uint8_t)(length >> 8)};
    uint64_t setup_phys = fast_virt_to_phys(setup);
    if (setup_phys > UINT32_MAX)
        goto fail;
    uint8_t status_pid = (request_type & 0x80U) ? 0U : 1U;
    uint8_t qtd_count = length ? 3U : 2U;
    memset(qtds, 0, PAGE_SIZE);
    ehci_qtd_prepare(&qtds[0], setup_phys, sizeof(setup),
        EHCI_QTD_PID_SETUP, 0, 0);
    if (length) {
        ehci_qtd_prepare(&qtds[1], data_phys, length,
            (request_type & 0x80U) ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT,
            1, 0);
        qtds[1].alternate = (uint32_t)fast_virt_to_phys(&qtds[2]);
        ehci_qtd_prepare(&qtds[2], 0, 0,
            status_pid ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT, 1, 1);
    } else {
        ehci_qtd_prepare(&qtds[1], 0, 0,
            EHCI_QTD_PID_IN, 1, 1);
    }

    int result = ehci_transfer(device, 0, device->ep0_packet, 0, qtds,
        qtd_count, 1);
    if (result == 0 && length && (request_type & 0x80U))
        memcpy(data, bounce, length);
    if (result == 0 && request == EHCI_REQUEST_SET_ADDRESS &&
        request_type == 0) {
        device->address = (uint8_t)value;
        if (device->device)
            device->device->address = device->address;
    }
    if (bounce_phys)
        ehci_dma_free(bounce_phys, bounce_pages * PAGE_SIZE);
    ehci_dma_free(qtd_phys, PAGE_SIZE);
    return result;

fail:
    if (bounce_phys)
        ehci_dma_free(bounce_phys, bounce_pages * PAGE_SIZE);
    if (qtd_phys)
        ehci_dma_free(qtd_phys, PAGE_SIZE);
    return -1;
}

int ehci_control_request(usb_device_t *device, uint8_t request_type,
    uint8_t request, uint16_t value, uint16_t index, void *data,
    uint16_t length) {
    ehci_device_t *entry = ehci_find_device(device);
    return entry ? ehci_control(entry, request_type, request, value, index,
                       data, length)
                 : -1;
}

int ehci_bulk_request(usb_device_t *device, uint8_t endpoint_address,
    void *buffer, uint32_t length, uint32_t *actual) {
    ehci_device_t *entry = ehci_find_device(device);
    if (!entry || !buffer || !length)
        return -1;
    if (actual)
        *actual = 0;
    usb_endpoint_t *endpoint = NULL;
    for (uint8_t i = 0; i < device->endpoint_count; ++i) {
        if (device->endpoints[i].address == endpoint_address) {
            endpoint = &device->endpoints[i];
            break;
        }
    }
    if (!endpoint || (endpoint->attributes & 3U) != 2U ||
        !endpoint->max_packet_size)
        return -1;

    uint32_t total = 0;
    while (total < length) {
        uint32_t chunk = length - total;
        if (chunk > 0x4000U)
            chunk = 0x4000U;
        size_t pages = (chunk + PAGE_SIZE - 1U) / PAGE_SIZE;
        uint64_t data_phys = allocate_pages_contiguous(pages);
        uint64_t qtd_phys = 0;
        ehci_qtd_t *qtd = ehci_dma_alloc(PAGE_SIZE, &qtd_phys);
        if (!data_phys || data_phys + pages * PAGE_SIZE - 1U > UINT32_MAX ||
            !qtd) {
            if (data_phys)
                ehci_dma_free(data_phys, pages * PAGE_SIZE);
            if (qtd_phys)
                ehci_dma_free(qtd_phys, PAGE_SIZE);
            return -1;
        }
        uint8_t *dma = paging_phys_to_virt((uintptr_t)data_phys);
        if (!(endpoint_address & 0x80U))
            memcpy(dma, (uint8_t *)buffer + total, chunk);
        ehci_qtd_prepare(qtd, data_phys, chunk,
            (endpoint_address & 0x80U) ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT,
            endpoint->data_toggle, 1);
        int result = ehci_transfer(entry, endpoint_address,
            endpoint->max_packet_size, endpoint->data_toggle, qtd, 1, 0);
        if (result == 0) {
            endpoint->data_toggle = (uint8_t)((entry->controller->transfer_qh->token &
                                                  EHCI_QTD_TOGGLE) != 0);
            uint32_t residual = (qtd->token >> 16) & 0x7FFFU;
            uint32_t transferred = chunk - residual;
            if ((endpoint_address & 0x80U) && transferred)
                memcpy((uint8_t *)buffer + total, dma, transferred);
            total += transferred;
            ehci_dma_free(data_phys, pages * PAGE_SIZE);
            ehci_dma_free(qtd_phys, PAGE_SIZE);
            if (transferred != chunk)
                break;
        } else {
            ehci_dma_free(data_phys, pages * PAGE_SIZE);
            ehci_dma_free(qtd_phys, PAGE_SIZE);
            return -1;
        }
    }
    if (actual)
        *actual = total;
    return 0;
}

int ehci_reset_endpoint(usb_device_t *device, uint8_t endpoint_address) {
    if (!device)
        return -1;
    for (uint8_t i = 0; i < device->endpoint_count; ++i) {
        if (device->endpoints[i].address == endpoint_address) {
            if ((device->endpoints[i].attributes & 3U) != 2U)
                return -1;
            device->endpoints[i].data_toggle = 0;
            return 0;
        }
    }
    return -1;
}

static int ehci_parse_config(usb_device_t *device) {
    uint8_t *bytes = device->configuration_descriptor;
    uint16_t length = device->configuration_length;
    int current = -1;
    for (uint16_t offset = 0; offset < length;) {
        if (length - offset < 2)
            return -1;
        uint8_t size = bytes[offset];
        uint8_t type = bytes[offset + 1];
        if (size < 2 || size > length - offset)
            return -1;
        if (type == EHCI_DESCRIPTOR_INTERFACE) {
            if (size < 9)
                return -1;
            current = -1;
            if (!bytes[offset + 3]) {
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
                    interface->interface_class == 8U &&
                    interface->interface_subclass == 6U &&
                    interface->interface_protocol == 0x50U;
                current = device->interface_count - 1;
            }
        } else if (type == EHCI_DESCRIPTOR_ENDPOINT) {
            if (size < 7)
                return -1;
            if (current >= 0) {
                if (device->endpoint_count >= USB_MAX_ENDPOINTS)
                    return -1;
                usb_endpoint_t *endpoint =
                    &device->endpoints[device->endpoint_count++];
                endpoint->address = bytes[offset + 2];
                endpoint->attributes = bytes[offset + 3];
                endpoint->max_packet_size =
                    (uint16_t)(bytes[offset + 4] |
                               ((uint16_t)bytes[offset + 5] << 8)) &
                    0x7FFU;
                endpoint->interval = bytes[offset + 6];
                endpoint->interface_number =
                    device->interfaces[current].number;
                uint8_t number = endpoint->address & 0x0FU;
                endpoint->endpoint_id = (uint8_t)(number * 2U +
                                                  ((endpoint->address & 0x80U) ? 1U : 0U));
            }
        }
        offset += size;
    }
    return device->interface_count ? 0 : -1;
}

static int ehci_hub_get_port_status(ehci_device_t *hub, uint8_t port,
    uint32_t *status) {
    uint8_t response[4] __attribute__((aligned(4))) = {0};
    if (!hub || !hub->device || !status ||
        ehci_control(hub, 0xA3U, EHCI_REQUEST_GET_STATUS, 0, port,
            response, sizeof(response)) != 0)
        return -1;
    *status = (uint32_t)response[0] | ((uint32_t)response[1] << 8) |
        ((uint32_t)response[2] << 16) | ((uint32_t)response[3] << 24);
    return 0;
}

static int ehci_hub_port_feature(ehci_device_t *hub, uint8_t port,
    uint8_t request, uint16_t feature) {
    return ehci_control(hub, 0x23U, request, feature, port, NULL, 0);
}

static int ehci_hub_initialize(ehci_device_t *entry) {
    uint8_t descriptor[9] __attribute__((aligned(4))) = {0};
    if (ehci_control(entry, 0xA0U, EHCI_REQUEST_GET_DESCRIPTOR,
            (uint16_t)(EHCI_DESCRIPTOR_HUB << 8), 0, descriptor,
            sizeof(descriptor)) != 0 ||
        descriptor[0] < sizeof(descriptor) ||
        descriptor[1] != EHCI_DESCRIPTOR_HUB ||
        !descriptor[2] || descriptor[2] > EHCI_HUB_MAX_PORTS) {
        error("Unsupported EHCI hub descriptor on address %u",
            __FILE__, entry->address);
        return -1;
    }

    entry->hub_port_count = descriptor[2];
    if ((descriptor[3] & 3U) != 0) {
        for (uint8_t port = 1; port <= entry->hub_port_count; ++port) {
            if (ehci_hub_port_feature(entry, port, EHCI_REQUEST_SET_FEATURE,
                    EHCI_HUB_PORT_POWER) != 0) {
                error("Unable to power EHCI hub %u port %u", __FILE__,
                    entry->address, port);
                return -1;
            }
        }
    }
    uint64_t power_ticks = ((uint64_t)descriptor[5] * 2U + 9U) / 10U;
    if (!power_ticks)
        power_ticks = 1;
    uint64_t power_deadline = pit_ticks + power_ticks;
    for (uint32_t wait = 0; wait < 100000000U &&
            pit_ticks < power_deadline; ++wait)
        __asm__ volatile("pause");
    entry->hub = 1;
    info("EHCI hub at address %u has %u powered ports", __FILE__,
        entry->address, entry->hub_port_count);
    return 0;
}

static int ehci_is_hub(const usb_device_t *device) {
    if (device->descriptor.device_class == EHCI_HUB_CLASS)
        return 1;
    for (uint8_t i = 0; i < device->interface_count; ++i) {
        if (device->interfaces[i].interface_class == EHCI_HUB_CLASS)
            return 1;
    }
    return 0;
}

static int ehci_enumerate(ehci_controller_t *ctrl, uint8_t root_port,
    ehci_device_t *parent_hub, uint8_t child_port) {
    const char *failure_stage = "port reset";
    uint8_t speed = 2U;
    if (!parent_hub) {
        volatile uint32_t *portsc =
            &ctrl->op[EHCI_PORTSC / 4 + root_port];
        uint32_t status = *portsc;
        if (!(status & EHCI_PORT_CONNECT))
            return -1;
        *portsc = (status & (EHCI_PORT_POWER | EHCI_PORT_OWNER)) |
                  EHCI_PORT_CHANGE | EHCI_PORT_POWER | EHCI_PORT_RESET;
        uint64_t reset_start = pit_ticks;
        for (uint32_t i = 0; i < 100000000U; ++i) {
            if (pit_ticks - reset_start >= 5U)
                break;
            __asm__ volatile("pause");
        }
        status = *portsc;
        *portsc = (status & (EHCI_PORT_POWER | EHCI_PORT_OWNER)) |
                  EHCI_PORT_CHANGE | EHCI_PORT_POWER;
        if (ehci_wait(portsc, EHCI_PORT_ENABLE, EHCI_PORT_ENABLE,
                10000000U) != 0) {
            status = *portsc;
            *portsc = (status & (EHCI_PORT_POWER | EHCI_PORT_OWNER)) |
                      EHCI_PORT_CHANGE | EHCI_PORT_OWNER;
            warn("EHCI port %u routed to its companion controller "
                 "(not supported)", __FILE__, root_port + 1U);
            return -1;
        }
    } else {
        uint32_t status = 0;
        if (ehci_hub_get_port_status(parent_hub, child_port, &status) != 0 ||
            !(status & EHCI_HUB_PORT_CONNECTION))
            return -1;
        if (ehci_hub_port_feature(parent_hub, child_port,
                EHCI_REQUEST_SET_FEATURE, EHCI_HUB_PORT_RESET) != 0)
            return -1;
        uint64_t reset_deadline = pit_ticks + 5U;
        for (uint32_t wait = 0; wait < 100000000U &&
                pit_ticks < reset_deadline; ++wait)
            __asm__ volatile("pause");
        uint64_t timeout = pit_ticks + 10U;
        do {
            if (ehci_hub_get_port_status(parent_hub, child_port,
                    &status) != 0)
                return -1;
            if (!(status & EHCI_HUB_PORT_RESETTING) &&
                (status & EHCI_HUB_PORT_ENABLE))
                break;
            __asm__ volatile("pause");
        } while (pit_ticks < timeout);
        if ((status & EHCI_HUB_PORT_RESETTING) ||
            !(status & EHCI_HUB_PORT_ENABLE)) {
            warn("EHCI hub %u port %u did not enable after reset",
                __FILE__, parent_hub->address, child_port);
            return -1;
        }
        if (status & EHCI_HUB_PORT_LOW_SPEED)
            speed = 1U;
        else if (status & EHCI_HUB_PORT_HIGH_SPEED)
            speed = 2U;
        else
            speed = 0U;
        (void)ehci_hub_port_feature(parent_hub, child_port,
            EHCI_REQUEST_CLEAR_FEATURE, EHCI_HUB_C_PORT_RESET);
    }

    ehci_device_t *entry = NULL;
    for (size_t i = 0; i < EHCI_MAX_DEVICES; ++i) {
        if (!ehci_devices[i].active) {
            entry = &ehci_devices[i];
            break;
        }
    }
    if (!entry) {
        error("No free EHCI device slots for root port %u", __FILE__,
            root_port + 1U);
        return -1;
    }
    memset(entry, 0, sizeof(*entry));
    entry->controller = ctrl;
    entry->port = root_port;
    entry->parent_hub = parent_hub;
    entry->parent_port = child_port;
    entry->speed = speed;
    entry->ep0_packet = speed == 2U ? 64U : 8U;
    entry->address = 0;
    entry->active = 1;

    uint64_t device_phys = 0;
    failure_stage = "device descriptor";
    uint8_t *descriptor = (uint8_t *)(uintptr_t)ehci_dma_alloc(PAGE_SIZE,
        &device_phys);
    if (!descriptor)
        goto fail;
    if (ehci_control(entry, 0x80, EHCI_REQUEST_GET_DESCRIPTOR, 0x0100, 0,
            descriptor, 8) != 0)
        goto fail;
    if (descriptor[0] < 8 || descriptor[1] != 1 ||
        (speed == 2U && descriptor[7] != 64U) ||
        (speed != 2U && descriptor[7] != 8U && descriptor[7] != 16U &&
            descriptor[7] != 32U && descriptor[7] != 64U))
        goto fail;
    entry->ep0_packet = descriptor[7];

    uint8_t address = ehci_allocate_address(ctrl);
    if (!address)
        goto fail;
    usb_device_t *device = kmalloc(sizeof(*device));
    if (!device)
        goto fail;
    memset(device, 0, sizeof(*device));
    device->host_type = USB_HOST_EHCI;
    device->root_port = root_port + 1U;
    device->speed = speed == 2U ? 3U : (speed == 1U ? 2U : 1U);
    device->address = address;
    entry->address = 0;
    entry->device = device;
    failure_stage = "SET_ADDRESS";
    if (ehci_control(entry, 0, EHCI_REQUEST_SET_ADDRESS, device->address, 0,
            NULL, 0) != 0) {
        entry->device = NULL;
        kfree(device);
        goto fail;
    }
    uint64_t address_start = pit_ticks;
    for (uint32_t i = 0; i < 20000000U; ++i) {
        if (pit_ticks - address_start >= 1U)
            break;
        __asm__ volatile("pause");
    }
    memcpy(descriptor, (uint8_t[18]){0}, 18);
    failure_stage = "addressed device descriptor";
    if (ehci_control(entry, 0x80, EHCI_REQUEST_GET_DESCRIPTOR, 0x0100, 0,
            descriptor, 18) != 0)
        goto device_fail;
    memcpy(&device->descriptor, descriptor, sizeof(device->descriptor));
    if (device->descriptor.length < sizeof(device->descriptor) ||
        device->descriptor.descriptor_type != 1 ||
        !device->descriptor.configuration_count) {
        error("Invalid EHCI device descriptor on port %u: "
              "length %u type %u class %u configurations %u",
            __FILE__, root_port + 1U, device->descriptor.length,
            device->descriptor.descriptor_type, device->descriptor.device_class,
            device->descriptor.configuration_count);
        goto device_fail;
    }

    failure_stage = "configuration descriptor header";
    if (ehci_control(entry, 0x80, EHCI_REQUEST_GET_DESCRIPTOR,
            (EHCI_DESCRIPTOR_CONFIGURATION << 8), 0, descriptor, 9) != 0 ||
        descriptor[0] < 9 ||
        descriptor[1] != EHCI_DESCRIPTOR_CONFIGURATION)
        goto device_fail;
    uint16_t config_length = (uint16_t)(descriptor[2] |
                                        ((uint16_t)descriptor[3] << 8));
    if (config_length < 9 || config_length > 4096)
        goto device_fail;
    device->configuration_descriptor = kmalloc_aligned(config_length,
        PAGE_SIZE);
    if (!device->configuration_descriptor)
        goto device_fail;
    device->configuration_length = config_length;
    failure_stage = "configuration descriptor";
    if (ehci_control(entry, 0x80, EHCI_REQUEST_GET_DESCRIPTOR,
            (EHCI_DESCRIPTOR_CONFIGURATION << 8), 0,
            device->configuration_descriptor, config_length) != 0 ||
        ehci_parse_config(device) != 0)
        goto device_fail;
    device->configuration_value = device->configuration_descriptor[5];
    failure_stage = "SET_CONFIGURATION";
    if (ehci_control(entry, 0, EHCI_REQUEST_SET_CONFIGURATION,
            device->configuration_value, 0, NULL, 0) != 0)
        goto device_fail;
    failure_stage = "class-driver registration";
    if (usb_device_connect(device) != 0)
        goto device_fail;
    if (ehci_is_hub(device) && ehci_hub_initialize(entry) != 0)
        warn("EHCI hub on root port %u was enumerated but is unusable",
            __FILE__, root_port + 1U);
    done("EHCI USB device enumerated on root port %u (address %u)",
        __FILE__, root_port + 1U, device->address);
    ehci_dma_free(device_phys, PAGE_SIZE);
    return 0;

device_fail:
    if (device->configuration_descriptor)
        kfree(device->configuration_descriptor);
    entry->device = NULL;
    kfree(device);
fail:
    error("EHCI enumeration failed on root port %u during %s", __FILE__,
        root_port + 1U, failure_stage);
    ehci_dma_free(device_phys, PAGE_SIZE);
    entry->active = 0;
    return -1;
}

static void ehci_remove_device_tree(ehci_device_t *entry) {
    for (size_t i = 0; i < EHCI_MAX_DEVICES; ++i) {
        ehci_device_t *child = &ehci_devices[i];
        if (child->active && child->parent_hub == entry)
            ehci_remove_device_tree(child);
    }
    if (entry->device) {
        usb_device_disconnect(entry->device);
        if (entry->device->configuration_descriptor)
            kfree(entry->device->configuration_descriptor);
        kfree(entry->device);
    }
    memset(entry, 0, sizeof(*entry));
}

static void ehci_poll_hubs(ehci_controller_t *ctrl) {
    for (size_t i = 0; i < EHCI_MAX_DEVICES; ++i) {
        ehci_device_t *hub = &ehci_devices[i];
        if (!hub->active || hub->controller != ctrl || !hub->hub)
            continue;
        if (pit_ticks - hub->hub_poll_tick < 10U)
            continue;
        hub->hub_poll_tick = pit_ticks;
        for (uint8_t port = 1; port <= hub->hub_port_count; ++port) {
            uint8_t index = port - 1U;
            uint32_t status = 0;
            if (ehci_hub_get_port_status(hub, port, &status) != 0) {
                warn("Unable to read EHCI hub %u port %u status",
                    __FILE__, hub->address, port);
                continue;
            }
            uint32_t changes = status >> 16;
            if (changes & (1U << 0))
                (void)ehci_hub_port_feature(hub, port,
                    EHCI_REQUEST_CLEAR_FEATURE,
                    EHCI_HUB_C_PORT_CONNECTION);
            if (changes & (1U << 1))
                (void)ehci_hub_port_feature(hub, port,
                    EHCI_REQUEST_CLEAR_FEATURE, EHCI_HUB_C_PORT_ENABLE);
            if (changes & (1U << 4))
                (void)ehci_hub_port_feature(hub, port,
                    EHCI_REQUEST_CLEAR_FEATURE, EHCI_HUB_C_PORT_RESET);

            uint8_t connected =
                !!(status & EHCI_HUB_PORT_CONNECTION);
            if (connected && !hub->hub_port_connected[index]) {
                hub->hub_port_connected[index] = 1;
                hub->hub_port_enumerated[index] = 0;
                hub->hub_port_attempts[index] = 0;
                hub->hub_port_retry_tick[index] = pit_ticks;
                info("EHCI hub %u port %u connected", __FILE__,
                    hub->address, port);
            }
            if (connected && hub->hub_port_connected[index] &&
                !hub->hub_port_enumerated[index] &&
                pit_ticks >= hub->hub_port_retry_tick[index]) {
                ++hub->hub_port_attempts[index];
                if (ehci_enumerate(ctrl, hub->port, hub, port) == 0) {
                    hub->hub_port_enumerated[index] = 1;
                } else if (hub->hub_port_attempts[index] < 5U) {
                    hub->hub_port_retry_tick[index] = pit_ticks + 100U;
                } else {
                    error("EHCI stopped retrying hub %u port %u after "
                          "%u attempts", __FILE__, hub->address, port,
                        hub->hub_port_attempts[index]);
                    hub->hub_port_enumerated[index] = 1;
                }
            } else if (!connected && hub->hub_port_connected[index]) {
                hub->hub_port_connected[index] = 0;
                hub->hub_port_enumerated[index] = 0;
                hub->hub_port_attempts[index] = 0;
                hub->hub_port_retry_tick[index] = 0;
                for (size_t child_index = 0;
                        child_index < EHCI_MAX_DEVICES; ++child_index) {
                    ehci_device_t *child = &ehci_devices[child_index];
                    if (child->active && child->parent_hub == hub &&
                        child->parent_port == port) {
                        ehci_remove_device_tree(child);
                        info("EHCI USB device disconnected from hub %u "
                             "port %u", __FILE__, hub->address, port);
                    }
                }
            }
        }
    }
}

void ehci_poll(void) {
    for (uint8_t c = 0; c < controller_count; ++c) {
        ehci_controller_t *ctrl = &controllers[c];
        if (!ctrl->initialized)
            continue;
        if (__sync_lock_test_and_set(&ctrl->poll_lock, 1))
            continue;
        for (uint32_t port = 0; port < ctrl->port_count && port < 16U; ++port) {
            volatile uint32_t *portsc = &ctrl->op[EHCI_PORTSC / 4 + port];
            uint32_t status = *portsc;
            uint8_t connected = !!(status & EHCI_PORT_CONNECT);
            if (status & EHCI_PORT_CHANGE)
                *portsc = (status & (EHCI_PORT_POWER | EHCI_PORT_OWNER)) |
                          (status & EHCI_PORT_CHANGE);
            if (connected && !(status & EHCI_PORT_OWNER) &&
                !ctrl->port_connected[port]) {
                ctrl->port_connected[port] = 1;
                ctrl->port_enumerated[port] = 0;
                ctrl->port_enumeration_attempts[port] = 0;
                ctrl->port_retry_tick[port] = pit_ticks;
                info("EHCI root port %u connected", __FILE__, port + 1U);
            }
            if (connected && !(status & EHCI_PORT_OWNER) &&
                ctrl->port_connected[port] &&
                !ctrl->port_enumerated[port] &&
                pit_ticks >= ctrl->port_retry_tick[port]) {
                ++ctrl->port_enumeration_attempts[port];
                if (ehci_enumerate(ctrl, (uint8_t)port, NULL, 0) == 0) {
                    ctrl->port_enumerated[port] = 1;
                } else if (ctrl->port_enumeration_attempts[port] < 5U) {
                    ctrl->port_retry_tick[port] = pit_ticks + 100U;
                } else {
                    error("EHCI stopped retrying root port %u after %u attempts",
                        __FILE__, port + 1U,
                        ctrl->port_enumeration_attempts[port]);
                    ctrl->port_enumerated[port] = 1;
                }
            } else if (!connected && ctrl->port_connected[port]) {
                ctrl->port_connected[port] = 0;
                ctrl->port_enumerated[port] = 0;
                ctrl->port_enumeration_attempts[port] = 0;
                ctrl->port_retry_tick[port] = 0;
                for (size_t i = 0; i < EHCI_MAX_DEVICES; ++i) {
                    ehci_device_t *entry = &ehci_devices[i];
                    if (!entry->active || entry->controller != ctrl ||
                        entry->port != port || entry->parent_hub)
                        continue;
                    ehci_remove_device_tree(entry);
                    info("EHCI USB device disconnected from port %u",
                        __FILE__, port + 1U);
                }
            }
        }
        ehci_poll_hubs(ctrl);
        __sync_lock_release(&ctrl->poll_lock);
    }
}

static int ehci_handoff(uint8_t bus, uint8_t slot, uint8_t function,
    uint32_t hccparams) {
    uint8_t offset = (uint8_t)((hccparams >> 8) & 0xFFU);
    if (offset < 0x40U)
        return 0;
    for (uint8_t i = 0; i < 48U && offset >= 0x40U; ++i) {
        uint32_t header = pci_config_read_dword(bus, slot, function,
            offset & 0xFCU);
        uint8_t id = (uint8_t)(header >> ((offset & 3U) * 8U));
        uint8_t next = (uint8_t)(header >> (((offset & 3U) + 1U) * 8U));
        if (id == 1U) {
            uint32_t sem = pci_config_read_dword(bus, slot, function,
                offset & 0xFCU);
            pci_config_write_dword(bus, slot, function, offset & 0xFCU,
                sem | (1U << ((offset & 3U) * 8U + 24U)));
            for (uint32_t wait = 0; wait < 10000000U; ++wait) {
                sem = pci_config_read_dword(bus, slot, function,
                    offset & 0xFCU);
                if (!(sem & (1U << ((offset & 3U) * 8U + 16U))))
                    return 0;
                __asm__ volatile("pause");
            }
            warn("EHCI BIOS ownership handoff timed out", __FILE__);
            return -1;
        }
        if (!next || next == offset)
            break;
        offset = next;
    }
    return 0;
}

void probe_ehci(uint8_t bus, uint8_t slot, uint8_t function) {
    for (uint8_t i = 0; i < controller_count; ++i) {
        if (controllers[i].bus == bus && controllers[i].slot == slot &&
            controllers[i].function == function)
            return;
    }
    if (controller_count >= EHCI_MAX_CONTROLLERS) {
        warn("EHCI controller limit reached", __FILE__);
        return;
    }
    uint32_t command = pci_config_read_dword(bus, slot, function, 0x04);
    pci_config_write_dword(bus, slot, function, 0x04,
        (command & 0xFFFF0000U) | ((command | 0x0006U) & 0xFFFFU));
    uint32_t bar_low = pci_config_read_dword(bus, slot, function, 0x10);
    uint32_t bar_type = (bar_low >> 1) & 3U;
    if (!bar_low || bar_low == 0xFFFFFFFFU || (bar_low & 1U) ||
        bar_type == 1U || bar_type == 3U) {
        error("Invalid EHCI MMIO BAR0", __FILE__);
        return;
    }
    uint64_t bar = bar_low & ~0xFULL;
    if (bar_type == 2U)
        bar |= (uint64_t)pci_config_read_dword(bus, slot, function, 0x14) << 32;
    if (bar > UINTPTR_MAX || !paging_map_mmio((uintptr_t)bar, PAGE_SIZE)) {
        error("Unable to map EHCI registers", __FILE__);
        return;
    }
    ehci_controller_t *ctrl = &controllers[controller_count];
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->bus = bus;
    ctrl->slot = slot;
    ctrl->function = function;
    ctrl->bar_phys = bar;
    ctrl->cap = paging_phys_to_virt((uintptr_t)bar);
    uint8_t cap_length = (uint8_t)ctrl->cap[EHCI_CAP_LENGTH / 4];
    uint32_t hcsparams = ctrl->cap[EHCI_HCSPARAMS / 4];
    uint32_t hccparams = ctrl->cap[EHCI_HCCPARAMS / 4];
    ctrl->port_count = hcsparams & 0x0FU;
    if (cap_length < 0x10U || !ctrl->port_count || ctrl->port_count > 15U ||
        !paging_map_mmio((uintptr_t)bar, cap_length + EHCI_PORTSC +
                                             ctrl->port_count * 4U)) {
        error("EHCI capability registers are invalid", __FILE__);
        return;
    }
    ctrl->op = (volatile uint32_t *)((uintptr_t)ctrl->cap + cap_length);
    if (ehci_handoff(bus, slot, function, hccparams) != 0)
        warn("Continuing with EHCI despite incomplete BIOS handoff", __FILE__);

    volatile uint32_t *cmd = &ctrl->op[EHCI_USBCMD / 4];
    volatile uint32_t *status = &ctrl->op[EHCI_USBSTS / 4];
    ctrl->op[EHCI_USBINTR / 4] = 0;
    *cmd &= ~(EHCI_CMD_RUN | EHCI_CMD_ASYNC_ENABLE);
    if (ehci_wait(status, EHCI_STS_HALTED, EHCI_STS_HALTED, 10000000U) != 0) {
        error("EHCI controller did not halt (USBCMD 0x%X USBSTS 0x%X)",
            __FILE__, *cmd, *status);
        return;
    }
    *cmd |= EHCI_CMD_RESET;
    if (ehci_wait(cmd, EHCI_CMD_RESET, 0, 10000000U) != 0) {
        error("EHCI controller reset timed out", __FILE__);
        return;
    }
    uint64_t qh_phys = 0, transfer_qh_phys = 0;
    ctrl->async_head = ehci_dma_alloc(PAGE_SIZE, &qh_phys);
    ctrl->transfer_qh = ehci_dma_alloc(PAGE_SIZE, &transfer_qh_phys);
    if (!ctrl->async_head || !ctrl->transfer_qh) {
        ehci_dma_free(qh_phys, PAGE_SIZE);
        ehci_dma_free(transfer_qh_phys, PAGE_SIZE);
        error("Unable to allocate EHCI asynchronous queue head", __FILE__);
        return;
    }
    ctrl->async_head_phys = qh_phys;
    ctrl->transfer_qh_phys = transfer_qh_phys;
    ctrl->async_head->horizontal =
        (uint32_t)transfer_qh_phys | EHCI_QH_LINK_TYPE;
    ctrl->async_head->endpoint_characteristics = EHCI_QH_HEAD;
    ctrl->async_head->next_qtd = EHCI_QTD_TERMINATE;
    ctrl->async_head->alternate_qtd = EHCI_QTD_TERMINATE;
    ctrl->transfer_qh->horizontal =
        (uint32_t)qh_phys | EHCI_QH_LINK_TYPE;
    ctrl->transfer_qh->next_qtd = EHCI_QTD_TERMINATE;
    ctrl->transfer_qh->alternate_qtd = EHCI_QTD_TERMINATE;
    ctrl->op[EHCI_CTRLDSSEGMENT / 4] = 0;
    ctrl->op[EHCI_ASYNCLISTADDR / 4] = (uint32_t)qh_phys;
    ctrl->op[EHCI_CONFIGFLAG / 4] = 1;
    ctrl->address_next = 0;
    ++controller_count;
    *cmd = EHCI_CMD_RUN | EHCI_CMD_ASYNC_ENABLE;
    if (ehci_wait(status, EHCI_STS_HALTED, 0, 10000000U) != 0 ||
        ehci_wait(status, EHCI_STS_ASYNC_STATUS, EHCI_STS_ASYNC_STATUS,
            10000000U) != 0) {
        *cmd &= ~(EHCI_CMD_RUN | EHCI_CMD_ASYNC_ENABLE);
        int stopped =
            ehci_wait(status, EHCI_STS_HALTED | EHCI_STS_ASYNC_STATUS,
                EHCI_STS_HALTED, 10000000U) == 0;
        --controller_count;
        if (stopped) {
            ehci_dma_free(qh_phys, PAGE_SIZE);
            ehci_dma_free(transfer_qh_phys, PAGE_SIZE);
        }
        ctrl->async_head = NULL;
        ctrl->transfer_qh = NULL;
        error("EHCI controller failed to start asynchronous schedule", __FILE__);
        if (!stopped)
            warn("EHCI schedule stop failed; retaining DMA memory", __FILE__);
        return;
    }
    ctrl->initialized = 1;
    ctrl->port_power_control = (uint8_t)((hcsparams >> 4) & 1U);
    if (ctrl->port_power_control) {
        for (uint32_t port = 0; port < ctrl->port_count; ++port) {
            volatile uint32_t *portsc = &ctrl->op[EHCI_PORTSC / 4 + port];
            uint32_t port_status = *portsc;
            *portsc = EHCI_PORT_POWER |
                      (port_status & (EHCI_PORT_OWNER | EHCI_PORT_CHANGE));
        }
        uint64_t power_start = pit_ticks;
        for (uint32_t wait = 0; wait < 250000000U; ++wait) {
            if (pit_ticks - power_start >= 10U)
                break;
            __asm__ volatile("pause");
        }
    }
    done("EHCI controller running: %u root ports at BAR 0x%X", __FILE__,
        ctrl->port_count, (uint32_t)bar);
    ehci_poll();
}
