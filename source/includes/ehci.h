/**
 * @file ehci.h
 * @brief EHCI USB 2.0 host-controller interface.
 */
#ifndef EHCI_H
#define EHCI_H

#include <usb.h>

/**
 * Probe and initialize a PCI class 0x0C/0x03/0x20 EHCI controller.
 *
 * @param bus PCI bus number.
 * @param slot PCI device slot.
 * @param function PCI function number.
 */
void probe_ehci(uint8_t bus, uint8_t slot, uint8_t function);

/**
 * Poll EHCI root ports for connection changes and service enumeration and
 * disconnect work. Call from non-interrupt context.
 */
void ehci_poll(void);

/**
 * Submit a synchronous endpoint-zero control request to an EHCI device.
 *
 * @param device Connected EHCI device.
 * @param request_type USB bmRequestType value.
 * @param request USB bRequest value.
 * @param value USB wValue value.
 * @param index USB wIndex value.
 * @param data Request payload buffer, or NULL when @p length is zero.
 * @param length Payload length in bytes.
 * @return 0 on success; a negative value if the device or transfer is invalid.
 */
int ehci_control_request(usb_device_t *device, uint8_t request_type,
    uint8_t request, uint16_t value, uint16_t index, void *data,
    uint16_t length);

/**
 * Submit a synchronous EHCI bulk transfer.
 *
 * @param device Connected EHCI device.
 * @param endpoint_address USB endpoint address from its descriptor.
 * @param buffer Transfer buffer.
 * @param length Requested transfer length in bytes.
 * @param actual Optional output for the number of bytes transferred.
 * @return 0 on success; a negative value on invalid input or transfer failure.
 */
int ehci_bulk_request(usb_device_t *device, uint8_t endpoint_address,
    void *buffer, uint32_t length, uint32_t *actual);

/**
 * Reset the EHCI data toggle for a bulk endpoint after clearing its halt
 * feature.
 *
 * @param device Connected EHCI device.
 * @param endpoint_address USB endpoint address from its descriptor.
 * @return 0 on success; -1 if the device or endpoint is not registered.
 */
int ehci_reset_endpoint(usb_device_t *device, uint8_t endpoint_address);

#endif
