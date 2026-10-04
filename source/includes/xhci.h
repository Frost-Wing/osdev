/**
 * @file xhci.h
 * @brief Minimal xHCI 1.2 host-controller interface.
 */
#ifndef XHCI_H
#define XHCI_H

#include <basics.h>
#include <isr.h>
#include <stdbool.h>
#include <stddef.h>

#define XHCI_MAX_CONTROLLERS 4
#define XHCI_TRB_RING_ENTRIES 256
#define USB_MAX_INTERFACES 16
#define USB_MAX_ENDPOINTS 32
#define USB_MAX_CLASS_DRIVERS 8

/** USB device descriptor, stored in host byte order. */
typedef struct __attribute__((packed)) {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t usb_version;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t max_packet_size0;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version;
    uint8_t manufacturer_index;
    uint8_t product_index;
    uint8_t serial_index;
    uint8_t configuration_count;
} usb_device_descriptor_t;

/** Interface descriptor information needed by class drivers. */
typedef struct {
    uint8_t number;
    uint8_t alternate_setting;
    uint8_t endpoint_count;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t string_index;
    bool mass_storage;
} usb_interface_t;

/** Endpoint descriptor information and its xHCI endpoint-context index. */
typedef struct {
    uint8_t address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
    uint8_t interface_number;
    uint8_t endpoint_id;
} usb_endpoint_t;

/**
 * A connected USB device enumerated on an xHCI root port.
 * The descriptor/configuration buffers and this structure are owned by xHCI;
 * callers must not retain pointers after the disconnect callback.
 */
typedef struct usb_device {
    uint8_t slot_id;
    uint8_t root_port;
    uint8_t speed;
    uint8_t configuration_value;
    uint16_t configuration_length;
    usb_device_descriptor_t descriptor;
    uint8_t *configuration_descriptor;
    uint8_t interface_count;
    uint8_t endpoint_count;
    usb_interface_t interfaces[USB_MAX_INTERFACES];
    usb_endpoint_t endpoints[USB_MAX_ENDPOINTS];
    uint64_t device_context_physical;
} usb_device_t;

/** Class-driver callback, invoked on connection and before disconnect cleanup. */
typedef void (*usb_class_driver_callback_t)(usb_device_t *device,
    const usb_interface_t *interface, bool connected);

/** xHCI transfer request block, laid out as required by the controller. */
typedef struct __attribute__((packed, aligned(16))) {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

/** Decoded fields from an xHCI command-completion event. */
typedef struct {
    uint8_t slot_id;
    uint8_t completion_code;
    uint8_t endpoint_id;
    uint32_t residual_length;
} xhci_completion_t;

/** Probe and initialize a PCI class 0x0C/0x03/0x30 controller. */
void probe_xhci(uint8_t bus, uint8_t slot, uint8_t function);
/**
 * Submit one command TRB and wait for its command-completion event.
 *
 * @param command_type xHCI command TRB type.
 * @param parameter_low Low 32 bits of the command parameter.
 * @param parameter_high High 32 bits of the command parameter.
 * @param control Command control bits, excluding type and cycle.
 * @param completion Optional completion result.
 * @return 0 on success; negative on invalid state, controller error, or timeout.
 */
int xhci_submit_command(uint8_t command_type, uint32_t parameter_low,
    uint32_t parameter_high, uint32_t control,
    xhci_completion_t *completion);
/**
 * Ring a controller doorbell.
 * @param slot_id Slot 0 for command ring; otherwise an enabled slot ID.
 * @param target Command target 0 or endpoint ID 1..31.
 * @return 0 on success, otherwise -1.
 */
int xhci_ring_doorbell(uint8_t slot_id, uint8_t target);
/**
 * Allocate a transfer ring.
 * @param slot_id Enabled device slot.
 * @param endpoint_id xHCI endpoint context index, 1..31.
 * @param dequeue_pointer Receives the physical dequeue pointer with DCS in bit 0.
 * @return 0 on success, otherwise -1.
 */
int xhci_create_transfer_ring(uint8_t slot_id, uint8_t endpoint_id,
    uint64_t *dequeue_pointer);
/**
 * Queue a single Normal TRB (DMA-safe buffer; endpoint context must be configured).
 * @return 0 on success, negative on invalid input or ring/doorbell failure.
 */
int xhci_queue_transfer(uint8_t slot_id, uint8_t endpoint_id,
    void *buffer, uint32_t length);
/**
 * Submit one synchronous bulk transfer.
 * @param actual Receives the transferred byte count, including short packets.
 */
int xhci_bulk_transfer(uint8_t slot_id, uint8_t endpoint_id, void *buffer,
    uint32_t length, uint32_t *actual);
/** Submit a synchronous endpoint-zero control request. */
int usb_control_request(usb_device_t *device, uint8_t request_type,
    uint8_t request, uint16_t value, uint16_t index, void *data,
    uint16_t length);
/** Reset an xHCI endpoint after clearing its USB halt feature. */
int xhci_reset_endpoint(uint8_t slot_id, uint8_t endpoint_id);
/** Allocate a slot using the xHCI Enable Slot command. */
int xhci_enable_slot(uint8_t *slot_id);

/**
 * Poll controller events and root-port status from non-interrupt context.
 * This services deferred connection/disconnection and enumeration work; call
 * periodically from non-interrupt context, including when interrupts are
 * available. This is also the event-polling fallback when interrupts are off.
 */
void xhci_poll(void);

/** @brief Handle an xHCI controller interrupt. */
void xhci_interrupt_handler(InterruptFrame *frame);

/**
 * Register a USB interface-class driver. A class code of 0xFF matches any
 * value for that field. Callbacks receive only matching interfaces and are
 * called with connected=false before device resources are released.
 *
 * @return 0 on success, -1 if the registry is full or callback is null.
 */
int usb_register_class_driver(uint8_t interface_class,
    uint8_t interface_subclass, uint8_t interface_protocol,
    usb_class_driver_callback_t callback);

/** Return the number of devices currently present in the USB registry. */
size_t usb_device_count(void);

/**
 * Return a registered device by zero-based registry index, or NULL if out of
 * range. The returned pointer is invalidated when that device disconnects.
 */
usb_device_t *usb_get_device(size_t index);

#endif
