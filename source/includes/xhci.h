/**
 * @file xhci.h
 * @brief Minimal xHCI 1.2 host-controller interface.
 */
#ifndef XHCI_H
#define XHCI_H

#include <basics.h>
#include <isr.h>

#define XHCI_MAX_CONTROLLERS 4
#define XHCI_TRB_RING_ENTRIES 256

typedef struct __attribute__((packed, aligned(16))) {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

typedef struct {
    uint8_t slot_id;
    uint8_t completion_code;
    uint8_t endpoint_id;
    uint32_t residual_length;
} xhci_completion_t;

/* Probe and initialize a PCI class 0x0C/0x03/0x30 controller. */
void probe_xhci(uint8_t bus, uint8_t slot, uint8_t function);
/* Submit one command TRB and wait for its command-completion event. */
int xhci_submit_command(uint8_t command_type, uint32_t parameter_low,
    uint32_t parameter_high, uint32_t control,
    xhci_completion_t *completion);
/* Ring slot 0/target 0 for commands, or a slot/endpoint target 1..31. */
int xhci_ring_doorbell(uint8_t slot_id, uint8_t target);
/* Allocate a transfer ring; return its physical dequeue pointer with DCS in bit 0. */
int xhci_create_transfer_ring(uint8_t slot_id, uint8_t endpoint_id,
    uint64_t *dequeue_pointer);
/* Queue a single Normal TRB (DMA-safe buffer; endpoint context must be configured). */
int xhci_queue_transfer(uint8_t slot_id, uint8_t endpoint_id,
    void *buffer, uint32_t length);
int xhci_enable_slot(uint8_t *slot_id);
/* Poll event/port registers periodically from non-interrupt context in fallback mode. */
void xhci_poll(void);
void xhci_interrupt_handler(InterruptFrame *frame);

#endif
