#ifndef NVME_H
#define NVME_H

#include <basics.h>

#define NVME_MAX_CONTROLLERS 8
#define NVME_MAX_NAMESPACES 16
#define NVME_ADMIN_QUEUE_DEPTH 16
#define NVME_IO_QUEUE_DEPTH 64

typedef volatile struct {
    uint64_t cap;
    uint32_t vs;
    uint32_t intms;
    uint32_t intmc;
    uint32_t cc;
    uint32_t rsv0;
    uint32_t csts;
    uint32_t nssr;
    uint32_t aqa;
    uint64_t asq;
    uint64_t acq;
} nvme_regs_t;

typedef struct __attribute__((packed)) {
    uint8_t opcode;
    uint8_t flags;
    uint16_t cid;
    uint32_t nsid;
    uint64_t rsv0;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} nvme_command_t;

typedef struct __attribute__((packed)) {
    uint32_t dw0;
    uint32_t rsv0;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} nvme_completion_t;

typedef struct {
    nvme_command_t* sq;
    nvme_completion_t* cq;
    uint16_t qid;
    uint16_t depth;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint8_t phase;
} nvme_queue_t;

typedef struct {
    nvme_regs_t* regs;
    uint32_t doorbell_stride;
    uint32_t controller_id;
    uint32_t nn;
    nvme_queue_t adminq;
    nvme_queue_t ioq;
    void* bounce_buffer;
    int present;
} nvme_controller_t;

typedef struct {
    int present;
    int controller_index;
    uint32_t nsid;
    uint64_t total_sectors;
    uint32_t lba_size;
    int logical_device;
    char name[32];
} nvme_namespace_t;

extern nvme_controller_t nvme_controllers[NVME_MAX_CONTROLLERS];
extern nvme_namespace_t nvme_namespaces[NVME_MAX_NAMESPACES];
extern int nvme_namespace_count;

void probe_nvme(uint8_t bus, uint8_t slot, uint8_t function);
int nvme_read_sector(int namespace_index, uint64_t lba, void* buffer, uint32_t count);
int nvme_write_sector(int namespace_index, uint64_t lba, void* buffer, uint32_t count);

#endif
