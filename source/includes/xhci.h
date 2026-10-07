/**
 * @file xhci.h
 * @brief xHCI host-controller interface.
 */
#ifndef XHCI_H
#define XHCI_H

#include <isr.h>
#include <usb.h>

#define XHCI_MAX_CONTROLLERS 4
#define XHCI_TRB_RING_ENTRIES 256

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

/**
 * Probe and initialize a PCI class 0x0C/0x03/0x30 xHCI controller.
 *
 * @param bus PCI bus number.
 * @param slot PCI device slot.
 * @param function PCI function number.
 */
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
 *
 * @param slot_id Slot 0 for the command ring; otherwise an enabled slot ID.
 * @param target Command target 0 or endpoint ID 1..31.
 * @return 0 on success, otherwise -1.
 */
int xhci_ring_doorbell(uint8_t slot_id, uint8_t target);

/**
 * Allocate a transfer ring.
 *
 * @param slot_id Enabled device slot.
 * @param endpoint_id xHCI endpoint context index, 1..31.
 * @param dequeue_pointer Receives the physical dequeue pointer with DCS in bit
 * 0.
 * @return 0 on success, otherwise -1.
 */
int xhci_create_transfer_ring(uint8_t slot_id, uint8_t endpoint_id,
    uint64_t *dequeue_pointer);

/**
 * Queue a single Normal TRB.
 *
 * The buffer must be DMA-safe and the endpoint context must already be
 * configured.
 *
 * @param slot_id Enabled device slot.
 * @param endpoint_id xHCI endpoint context index, 1..31.
 * @param buffer DMA-safe transfer buffer.
 * @param length Transfer length in bytes.
 * @return 0 on success, negative on invalid input or ring/doorbell failure.
 */
int xhci_queue_transfer(uint8_t slot_id, uint8_t endpoint_id,
    void *buffer, uint32_t length);

/**
 * Submit one synchronous xHCI bulk transfer.
 *
 * @param slot_id Enabled device slot.
 * @param endpoint_id xHCI endpoint context index, 1..31.
 * @param buffer DMA-safe transfer buffer.
 * @param length Requested transfer length in bytes.
 * @param actual Receives the transferred byte count, including short packets.
 * @return 0 on success; a negative value on invalid input or transfer failure.
 */
int xhci_bulk_transfer(uint8_t slot_id, uint8_t endpoint_id, void *buffer,
    uint32_t length, uint32_t *actual);

/**
 * Reset an xHCI endpoint after its USB halt feature has been cleared.
 *
 * @param slot_id Enabled device slot.
 * @param endpoint_id xHCI endpoint context index, 1..31.
 * @return 0 on success, otherwise -1.
 */
int xhci_reset_endpoint(uint8_t slot_id, uint8_t endpoint_id);

/**
 * Allocate a slot using the xHCI Enable Slot command.
 *
 * @param slot_id Receives the enabled slot ID.
 * @return 0 on success, otherwise -1.
 */
int xhci_enable_slot(uint8_t *slot_id);

/**
 * Poll xHCI events and root-port status from non-interrupt context.
 *
 * This services deferred connection/disconnection and enumeration work. Call
 * periodically from non-interrupt context, including when interrupts are
 * available. It also polls EHCI root ports as the USB polling entry point.
 */
void xhci_poll(void);

/** Handle an xHCI controller interrupt. */
void xhci_interrupt_handler(InterruptFrame *frame);

#endif
