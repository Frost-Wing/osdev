/**
 * @file nvme.c
 * @brief Minimal polled NVMe support for FrostWing block devices.
 */

#include <nvme.h>
#include <pci.h>
#include <ahci.h>
#include <disk/gpt.h>
#include <disk/mbr.h>
#include <heap.h>
#include <graphics.h>

#define NVME_CC_EN            (1U << 0)
#define NVME_CC_CSS_NVM       (0U << 4)
#define NVME_CC_MPS_SHIFT     7
#define NVME_CC_AMS_RR        (0U << 11)
#define NVME_CC_IOSQES_SHIFT  16
#define NVME_CC_IOCQES_SHIFT  20
#define NVME_CSTS_RDY         (1U << 0)
#define NVME_ADMIN_OP_CREATE_IO_SQ 0x01
#define NVME_ADMIN_OP_CREATE_IO_CQ 0x05
#define NVME_ADMIN_OP_IDENTIFY     0x06
#define NVME_ADMIN_OP_SET_FEATURES 0x09
#define NVME_NVM_OP_WRITE          0x01
#define NVME_NVM_OP_READ           0x02
#define NVME_FEAT_NUM_QUEUES       0x07

nvme_controller_t nvme_controllers[NVME_MAX_CONTROLLERS];
nvme_namespace_t nvme_namespaces[NVME_MAX_NAMESPACES];
int nvme_namespace_count = 0;

static inline volatile uint32_t* nvme_sq_doorbell(nvme_controller_t* ctrl, uint16_t qid)
{
    uintptr_t base = (uintptr_t)ctrl->regs + 0x1000 + (2 * qid) * ctrl->doorbell_stride;
    return (volatile uint32_t*)base;
}

static inline volatile uint32_t* nvme_cq_doorbell(nvme_controller_t* ctrl, uint16_t qid)
{
    uintptr_t base = (uintptr_t)ctrl->regs + 0x1000 + (2 * qid + 1) * ctrl->doorbell_stride;
    return (volatile uint32_t*)base;
}

static int nvme_wait_ready(nvme_controller_t* ctrl, int ready, int timeout_ms)
{
    for (int i = 0; i < timeout_ms * 1000; i++) { // spin per microsecond
        int is_ready = (ctrl->regs->csts & NVME_CSTS_RDY) ? 1 : 0;
        if (is_ready == ready)
            return 0;
        io_wait_us(1); // small delay to not hammer the bus
    }
    return -1;
}


static int nvme_submit_and_wait(nvme_controller_t* ctrl, nvme_queue_t* q, nvme_command_t* cmd)
{
    uint16_t cid = q->sq_tail;
    cmd->cid = cid;
    q->sq[q->sq_tail] = *cmd;

    q->sq_tail = (q->sq_tail + 1) % q->depth;
    *nvme_sq_doorbell(ctrl, q->qid) = q->sq_tail;

    for (int spin = 0; spin < 1000000; spin++) {
        nvme_completion_t* cpl = &q->cq[q->cq_head];
        if ((cpl->status & 1) != q->phase)
            continue;

        uint16_t status = cpl->status >> 1;
        if (cpl->cid != cid || status != 0)
            return -1;

        q->cq_head++;
        if (q->cq_head == q->depth) {
            q->cq_head = 0;
            q->phase ^= 1;
        }

        *nvme_cq_doorbell(ctrl, q->qid) = q->cq_head;
        return 0;
    }

    return -1;
}

static int nvme_identify(nvme_controller_t* ctrl, uint32_t nsid, uint32_t cns, void* buffer)
{
    nvme_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_OP_IDENTIFY;
    cmd.nsid = nsid;
    cmd.prp1 = (uint64_t)(uintptr_t)buffer;
    cmd.cdw10 = cns;

    return nvme_submit_and_wait(ctrl, &ctrl->adminq, &cmd);
}

static int nvme_create_io_queues(nvme_controller_t* ctrl)
{
    nvme_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    // Set Features: Number of Queues
    cmd.opcode = NVME_ADMIN_OP_SET_FEATURES;
    cmd.cdw10 = NVME_FEAT_NUM_QUEUES;
    cmd.cdw11 = 0; // 1 IO queue
    if (nvme_submit_and_wait(ctrl, &ctrl->adminq, &cmd) != 0)
        return -2;

    // Create IO Completion Queue
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_OP_CREATE_IO_CQ;
    cmd.prp1 = (uint64_t)(uintptr_t)ctrl->ioq.cq;
    cmd.cdw10 = ((ctrl->ioq.depth - 1) << 16) | ctrl->ioq.qid;
    cmd.cdw11 = 0x1; // phase = 1
    if (nvme_submit_and_wait(ctrl, &ctrl->adminq, &cmd) != 0)
        return -3;

    // Create IO Submission Queue
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_OP_CREATE_IO_SQ;
    cmd.prp1 = (uint64_t)(uintptr_t)ctrl->ioq.sq;
    cmd.cdw10 = ((ctrl->ioq.depth - 1) << 16) | ctrl->ioq.qid;
    cmd.cdw11 = (ctrl->ioq.qid << 16) | 0x1; // CQID + PC=1
    if (nvme_submit_and_wait(ctrl, &ctrl->adminq, &cmd) != 0)
        return -4;

    return 0;
}


static int nvme_init_controller(nvme_controller_t* ctrl)
{
    uint16_t max_entries = (uint16_t)((ctrl->regs->cap & 0xFFFF) + 1);
    uint8_t dstrd = (ctrl->regs->cap >> 32) & 0xF;
    uint8_t mpsmin = (ctrl->regs->cap >> 48) & 0xF;
    uint8_t mpsmax = (ctrl->regs->cap >> 52) & 0xF;
    uint8_t page_shift = 12;
    uint8_t mps = page_shift - 12;

    if (mps < mpsmin || mps > mpsmax) {
        warn("[NVMe] Controller page size is unsupported", __FILE__);
        return -1;
    }

    ctrl->doorbell_stride = 4U << dstrd;

    ctrl->regs->cc &= ~NVME_CC_EN;
    if (nvme_wait_ready(ctrl, 0, 500) != 0) {
        warn("[NVMe] Controller did not disable", __FILE__);
        return -2;
    }

    ctrl->adminq.qid = 0;
    ctrl->adminq.depth = NVME_ADMIN_QUEUE_DEPTH;
    if (ctrl->adminq.depth > max_entries)
        ctrl->adminq.depth = max_entries;

    ctrl->adminq.sq = kmalloc_aligned(sizeof(nvme_command_t) * ctrl->adminq.depth, 4096);
    ctrl->adminq.cq = kmalloc_aligned(sizeof(nvme_completion_t) * ctrl->adminq.depth, 4096);
    if (!ctrl->adminq.sq || !ctrl->adminq.cq)
        return -2;

    memset(ctrl->adminq.sq, 0, sizeof(nvme_command_t) * ctrl->adminq.depth);
    memset(ctrl->adminq.cq, 0, sizeof(nvme_completion_t) * ctrl->adminq.depth);
    ctrl->adminq.sq_tail = 0;
    ctrl->adminq.cq_head = 0;
    ctrl->adminq.phase = 1;

    ctrl->regs->aqa = ((ctrl->adminq.depth - 1) << 16) | (ctrl->adminq.depth - 1);
    ctrl->regs->asq = (uint64_t)(uintptr_t)ctrl->adminq.sq;
    ctrl->regs->acq = (uint64_t)(uintptr_t)ctrl->adminq.cq;

    ctrl->regs->cc =
        NVME_CC_EN |
        NVME_CC_CSS_NVM |
        ((uint32_t)mps << NVME_CC_MPS_SHIFT) |
        NVME_CC_AMS_RR |
        (6U << NVME_CC_IOSQES_SHIFT) |
        (4U << NVME_CC_IOCQES_SHIFT);

    if (nvme_wait_ready(ctrl, 1, 500) != 0) {
        warn("[NVMe] Controller did not become ready", __FILE__);
        return -3;
    }

    ctrl->ioq.qid = 1;
    ctrl->ioq.depth = NVME_IO_QUEUE_DEPTH;
    if (ctrl->ioq.depth > max_entries)
        ctrl->ioq.depth = max_entries;

    ctrl->ioq.sq = kmalloc_aligned(sizeof(nvme_command_t) * ctrl->ioq.depth, 4096);
    ctrl->ioq.cq = kmalloc_aligned(sizeof(nvme_completion_t) * ctrl->ioq.depth, 4096);
    ctrl->bounce_buffer = kmalloc_aligned(4096, 4096);
    if (!ctrl->ioq.sq || !ctrl->ioq.cq || !ctrl->bounce_buffer)
        return -4;

    memset(ctrl->ioq.sq, 0, sizeof(nvme_command_t) * ctrl->ioq.depth);
    memset(ctrl->ioq.cq, 0, sizeof(nvme_completion_t) * ctrl->ioq.depth);
    ctrl->ioq.sq_tail = 0;
    ctrl->ioq.cq_head = 0;
    ctrl->ioq.phase = 1;

    return nvme_create_io_queues(ctrl);
}

static int nvme_buffer_needs_bounce(void* buffer, uint32_t lba_size)
{
    uintptr_t start = (uintptr_t)buffer;
    uintptr_t end = start + lba_size;

    if (start < heap_begin || start >= heap_end)
        return 1;

    return ((start & 0xFFF) + lba_size) > 4096 || end > heap_end;
}

static int nvme_read_write_one(nvme_namespace_t* ns, uint64_t lba, void* buffer, int is_write)
{
    nvme_controller_t* ctrl = &nvme_controllers[ns->controller_index];
    nvme_command_t cmd;
    uint8_t* io_buffer = (uint8_t*)buffer;
    int use_bounce = nvme_buffer_needs_bounce(buffer, ns->lba_size);

    if (use_bounce) {
        /*
         * Filesystem probing commonly reads into stack buffers, while early
         * partition probing often uses aligned heap buffers. Bounce only when
         * the caller buffer is not DMA-safe for the controller.
         */
        if (!ctrl->bounce_buffer)
            return -1;

        io_buffer = (uint8_t*)ctrl->bounce_buffer;
        if (is_write)
            memcpy(io_buffer, buffer, ns->lba_size);
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = is_write ? NVME_NVM_OP_WRITE : NVME_NVM_OP_READ;
    cmd.nsid = ns->nsid;
    cmd.prp1 = (uint64_t)(uintptr_t)io_buffer;
    cmd.cdw10 = (uint32_t)lba;
    cmd.cdw11 = (uint32_t)(lba >> 32);
    cmd.cdw12 = 0;

    int rc = nvme_submit_and_wait(ctrl, &ctrl->ioq, &cmd);
    if (rc == 0 && use_bounce && !is_write)
        memcpy(buffer, io_buffer, ns->lba_size);

    return rc;
}

static void nvme_probe_namespaces(int controller_index)
{
    nvme_controller_t* ctrl = &nvme_controllers[controller_index];
    uint8_t* identify_buf = kmalloc_aligned(4096, 4096);
    if (!identify_buf)
        return;

    if (nvme_identify(ctrl, 0, 1, identify_buf) != 0) {
        warn("[NVMe] Identify controller failed", __FILE__);
        kfree(identify_buf);
        return;
    }

    ctrl->nn = ((uint32_t*)identify_buf)[129];
    if (ctrl->nn == 0)
        ctrl->nn = 1;

    for (uint32_t nsid = 1; nsid <= ctrl->nn && nvme_namespace_count < NVME_MAX_NAMESPACES; nsid++) {
        memset(identify_buf, 0, 4096);
        if (nvme_identify(ctrl, nsid, 0, identify_buf) != 0)
            continue;

        uint64_t nsze = *(uint64_t*)(identify_buf + 0);
        if (nsze == 0)
            continue;

        uint8_t flbas = identify_buf[26] & 0x0F;
        uint32_t lbaf = *(uint32_t*)(identify_buf + 128 + flbas * 4);
        uint8_t lbads = (lbaf >> 16) & 0xFF;
        uint32_t lba_size = 1U << lbads;

        if (lba_size != SECTOR_SIZE) {
            printf("[NVMe] skipping namespace %u with unsupported LBA size %u", nsid, lba_size);
            continue;
        }

        int ns_index = nvme_namespace_count++;
        nvme_namespace_t* ns = &nvme_namespaces[ns_index];
        memset(ns, 0, sizeof(*ns));

        ns->present = 1;
        ns->controller_index = controller_index;
        ns->nsid = nsid;
        ns->total_sectors = nsze;
        ns->lba_size = lba_size;
        snprintf(ns->name, sizeof(ns->name), "nvme%dn%u", controller_index, nsid);
        ns->logical_device = block_register_device(
            BLOCK_DEVICE_NVME,
            ns_index,
            ns->total_sectors,
            ns->lba_size,
            ns->name
        );

        if (ns->logical_device < 0)
            continue;

        printf("[NVMe] namespace %s detected (%u sectors)", ns->name, (uint32_t)ns->total_sectors);

        if (check_gpt(ns->logical_device) != 0)
            check_mbr(ns->logical_device);
    }

    kfree(identify_buf);
}

void probe_nvme(uint8_t bus, uint8_t slot, uint8_t function)
{
    for (int i = 0; i < NVME_MAX_CONTROLLERS; i++) {
        nvme_controller_t* ctrl = &nvme_controllers[i];
        if (ctrl->present)
            continue;

        uint32_t command = pci_config_read_dword(bus, slot, function, 0x04);
        command |= 0x00000006U; /* bus master + memory space */
        pci_config_write_dword(bus, slot, function, 0x04, command);

        uint64_t bar = (uint32_t)(pci_config_read_dword(bus, slot, function, 0x10) & ~0xF);
        uint32_t upper = pci_config_read_dword(bus, slot, function, 0x14);
        if (upper && upper != 0xFFFFFFFFU)
            bar |= ((uint64_t)upper << 32);

        if (!bar || bar == 0xFFFFFFFFULL) {
            warn("[NVMe] Failed to find MMIO BAR", __FILE__);
            return;
        }

        memset(ctrl, 0, sizeof(*ctrl));
        ctrl->regs = (nvme_regs_t*)(uintptr_t)bar;
        ctrl->controller_id = i;

        if (nvme_init_controller(ctrl) != 0) {
            printf("[NVMe] init failure: CAP=0x%X:%X CC=0x%X CSTS=0x%X",
                (uint32_t)(ctrl->regs->cap >> 32),
                (uint32_t)(ctrl->regs->cap & 0xFFFFFFFFU),
                ctrl->regs->cc,
                ctrl->regs->csts);
            warn("[NVMe] Controller init failed", __FILE__);
            hcf2();
            return;
        }

        ctrl->present = 1;
        printf("[NVMe] controller %d ready", i);
        nvme_probe_namespaces(i);
        return;
    }
}

int nvme_read_sector(int namespace_index, uint64_t lba, void* buffer, uint32_t count)
{
    if (namespace_index < 0 || namespace_index >= nvme_namespace_count || !buffer)
        return -1;

    nvme_namespace_t* ns = &nvme_namespaces[namespace_index];
    if (!ns->present)
        return -1;

    uint8_t* ptr = (uint8_t*)buffer;
    for (uint32_t i = 0; i < count; i++) {
        if (nvme_read_write_one(ns, lba + i, ptr + i * ns->lba_size, 0) != 0)
            return -2;
    }

    return 0;
}

int nvme_write_sector(int namespace_index, uint64_t lba, void* buffer, uint32_t count)
{
    if (namespace_index < 0 || namespace_index >= nvme_namespace_count || !buffer)
        return -1;

    nvme_namespace_t* ns = &nvme_namespaces[namespace_index];
    if (!ns->present)
        return -1;

    uint8_t* ptr = (uint8_t*)buffer;
    for (uint32_t i = 0; i < count; i++) {
        if (nvme_read_write_one(ns, lba + i, ptr + i * ns->lba_size, 1) != 0)
            return -2;
    }

    return 0;
}
