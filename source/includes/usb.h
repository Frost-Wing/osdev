/**
 * @file usb.h
 * @brief Shared USB device, descriptor, and class-driver interfaces.
 */
#ifndef USB_H
#define USB_H

#include <basics.h>
#include <stdbool.h>
#include <stddef.h>

#define USB_MAX_INTERFACES 16
#define USB_MAX_ENDPOINTS 32
#define USB_MAX_CLASS_DRIVERS 8

/** Host-controller backend currently servicing a USB device. */
typedef enum {
    USB_HOST_XHCI = 0,
    USB_HOST_EHCI = 1
} usb_host_type_t;

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

/** Interface descriptor information exposed to USB class drivers. */
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

/** Endpoint descriptor information used by USB class drivers. */
typedef struct {
    uint8_t address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
    uint8_t interface_number;
    uint8_t endpoint_id;
    uint8_t data_toggle;
} usb_endpoint_t;

/**
 * A connected USB device enumerated by a supported host controller.
 *
 * The descriptor/configuration buffers and this structure are owned by the
 * host controller; callers must not retain pointers after disconnect.
 */
typedef struct usb_device {
    usb_host_type_t host_type;
    uint8_t slot_id;
    uint8_t root_port;
    uint8_t speed;
    uint8_t address;
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

/**
 * USB interface-class callback, invoked on connection and before disconnect
 * cleanup.
 */
typedef void (*usb_class_driver_callback_t)(usb_device_t *device,
    const usb_interface_t *interface, bool connected);

/**
 * Submit a synchronous endpoint-zero control request through the device's
 * host controller.
 *
 * @param device Connected USB device.
 * @param request_type USB bmRequestType value.
 * @param request USB bRequest value.
 * @param value USB wValue value.
 * @param index USB wIndex value.
 * @param data Request payload buffer, or NULL when @p length is zero.
 * @param length Payload length in bytes.
 * @return 0 on success; a negative value on invalid input or transfer failure.
 */
int usb_control_request(usb_device_t *device, uint8_t request_type,
    uint8_t request, uint16_t value, uint16_t index, void *data,
    uint16_t length);

/**
 * Submit a bulk transfer through the device's host controller.
 *
 * @param device Connected USB device.
 * @param endpoint_address USB endpoint address from its descriptor.
 * @param endpoint_id Host-controller endpoint identifier (xHCI only).
 * @param buffer Transfer buffer.
 * @param length Requested transfer length in bytes.
 * @param actual Optional output for the number of bytes transferred.
 * @return 0 on success; a negative value on invalid input or transfer failure.
 */
int usb_bulk_request(usb_device_t *device, uint8_t endpoint_address,
    uint8_t endpoint_id, void *buffer, uint32_t length, uint32_t *actual);

/**
 * Reset a halted bulk endpoint after its USB halt feature has been cleared.
 *
 * @param device Connected USB device.
 * @param endpoint_address USB endpoint address from its descriptor.
 * @param endpoint_id Host-controller endpoint identifier (xHCI only).
 * @return 0 on success; a negative value if the endpoint cannot be reset.
 */
int usb_reset_endpoint(usb_device_t *device, uint8_t endpoint_address,
    uint8_t endpoint_id);

/**
 * Publish an enumerated device and notify matching registered class drivers.
 *
 * @param device Device to publish.
 * @return 0 on success; -1 if the device is invalid or the registry is full.
 */
int usb_device_connect(usb_device_t *device);

/**
 * Notify matching class drivers of disconnect and remove the device.
 *
 * The device and its owned buffers remain the host controller's
 * responsibility and may be released after this call returns.
 *
 * @param device Device being disconnected; NULL is ignored.
 */
void usb_device_disconnect(usb_device_t *device);

/**
 * Register a USB interface-class driver. A class code of 0xFF matches any
 * value for that field. Callbacks receive only matching interfaces and are
 * called with connected=false before device resources are released.
 *
 * @param interface_class Interface class to match, or 0xFF for any.
 * @param interface_subclass Interface subclass to match, or 0xFF for any.
 * @param interface_protocol Interface protocol to match, or 0xFF for any.
 * @param callback Callback to invoke for matching interfaces.
 * @return 0 on success, -1 if the registry is full or callback is null.
 */
int usb_register_class_driver(uint8_t interface_class,
    uint8_t interface_subclass, uint8_t interface_protocol,
    usb_class_driver_callback_t callback);

/** @return Number of devices currently present in the USB registry. */
size_t usb_device_count(void);

/**
 * Return a registered device by zero-based registry index.
 *
 * @param index Zero-based registry index.
 * @return Device pointer, or NULL if @p index is out of range. The pointer is
 * invalidated when that device disconnects.
 */
usb_device_t *usb_get_device(size_t index);

#endif
