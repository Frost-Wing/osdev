/**
 * @file memory.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief All memory based functions
 * @version 0.1
 * @date 2023-10-21
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <memory.h>
#include <graphics.h>

/*
 * GCC and Clang reserve the right to generate calls to the following
 * 4 functions even if they are not directly called.
 * Implement them as the C specification mandates.
 * DO NOT remove or rename these functions, or stuff will eventually break!
 * They CAN be moved to a different .c file.
 */

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
 
    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }
 
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
 
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
 
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
 
    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }
 
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
 
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }
 
    return 0;
}

void memory_dump(const void* start, const void* end) {
    const unsigned char* ptr = (const unsigned char*)start;

    while (ptr <= (const unsigned char*)end) {
        printf("Address: 0x%x Value: 0x%x", (void*)ptr, *ptr);
        ptr++;
    }
}

void registers_dump(){
    unsigned long long rax_value, rbx_value, rcx_value, rdx_value, rsi_value, rdi_value, 
                       rbp_value, rsp_value, r8_value, r9_value, r10_value, r11_value, 
                       r12_value, r13_value, r14_value, r15_value;

    unsigned long long rip_value;
    asm volatile(
        "leaq (%%rip), %0"
        : "=r"(rip_value)
    );

    asm("movq %%rax, %0" : "=r" (rax_value));
    asm("movq %%rbx, %0" : "=r" (rbx_value));
    asm("movq %%rcx, %0" : "=r" (rcx_value));
    asm("movq %%rdx, %0" : "=r" (rdx_value));
    asm("movq %%rsi, %0" : "=r" (rsi_value));
    asm("movq %%rdi, %0" : "=r" (rdi_value));
    asm("movq %%rbp, %0" : "=r" (rbp_value));
    asm("movq %%rsp, %0" : "=r" (rsp_value));
    asm("movq %%r8, %0" : "=r" (r8_value));
    asm("movq %%r9, %0" : "=r" (r9_value));
    asm("movq %%r10, %0" : "=r" (r10_value));
    asm("movq %%r11, %0" : "=r" (r11_value));
    asm("movq %%r12, %0" : "=r" (r12_value));
    asm("movq %%r13, %0" : "=r" (r13_value));
    asm("movq %%r14, %0" : "=r" (r14_value));
    asm("movq %%r15, %0" : "=r" (r15_value));

    print("=[ Register Dump ]=\n");
    printf("        RIP = 0x%x", rip_value);
    printf("        RAX = 0x%x", rax_value);
    printf("        RBX = 0x%x", rbx_value);
    printf("        RCX = 0x%x", rcx_value);
    printf("        RDX = 0x%x", rdx_value);
    printf("        RSI = 0x%x", rsi_value);
    printf("        RDI = 0x%x", rdi_value);
    printf("        RBP = 0x%x", rbp_value);
    printf("        RSP = 0x%x", rsp_value);
    printf("        R08 = 0x%x", r8_value);
    printf("        R09 = 0x%x", r9_value);
    printf("        R10 = 0x%x", r10_value);
    printf("        R11 = 0x%x", r11_value);
    printf("        R12 = 0x%x", r12_value);
    printf("        R13 = 0x%x", r13_value);
    printf("        R14 = 0x%x", r14_value);
    printf("        R15 = 0x%x", r15_value);

    return;
}

void* allocate_memory_at_address(int64 phys_addr, size_t size) {
    for(int64 i = phys_addr; i < phys_addr + size; i++){
        int64* ptr_i = (int64*)i;
        int64 value_at_i = *ptr_i;
        if(value_at_i != 0){
            error("The memory block you requested is being used!", __FILE__);
            return null;
        }
    }

    void* ptr = (void*)phys_addr;

    return ptr;
}

void display_memory_formatted(struct memory_context* memory) {
    printf("Usable                 : %05d KiB", memory->usable / 1024);
    printf("Reserved               : %05d KiB", memory->reserved / 1024);
    printf("ACPI Reclaimable       : %05d KiB", memory->acpi_reclaimable / 1024);
    printf("ACPI NVS               : %05d KiB", memory->acpi_nvs / 1024);
    printf("Bad                    : %05d KiB", memory->bad / 1024);
    printf("Bootloader Reclaimable : %05d KiB", memory->bootloader_reclaimable / 1024);
    printf("Kernel Modules         : %05d KiB", memory->kernel_modules / 1024);
    printf("Framebuffer            : %05d KiB", memory->framebuffer / 1024);
    printf("Unknown                : %05d KiB", memory->unknown / 1024);   print(yellow_color);
    printf("Grand Total            : %d MiB", ((memory->total / 1024)/1024)-3); // There is an error of 3MB (approx.) beacuse of division.
}

void analyze_memory_map(struct memory_context* memory, struct limine_memmap_request memory_map_request){

    memory->total = 0;
    memory->usable = 0;
    memory->reserved = 0;
    memory->acpi_reclaimable = 0;
    memory->acpi_nvs = 0;
    memory->bad = 0;
    memory->bootloader_reclaimable = 0;
    memory->kernel_modules = 0;
    memory->framebuffer = 0;
    memory->unknown = 0;

    for (size_t i = 0; i < memory_map_request.response->entry_count; ++i) {
        int length = memory_map_request.response->entries[i]->length;
        memory->total += length;
        string type = "";
        switch (memory_map_request.response->entries[i]->type)
        {
            case 0:
                memory->usable += length;
                type = "Usable";
                break;
            case 1:
                memory->reserved += length;
                type = "Reserved";
                break;
            case 2:
                memory->acpi_reclaimable += length;
                type = "ACPI Reclaimable";
                break;
            case 3:
                memory->acpi_nvs += length;
                type = "ACPI NVS";
                break;
            case 4:
                memory->bad += length;
                type = "Faulty";
                break;
            case 5:
                memory->bootloader_reclaimable += length;
                type = "Bootloader Reclaimable";
                break;
            case 6:
                memory->kernel_modules += length;
                type = "Kernel & Modules";
                break;
            case 7:
                memory->framebuffer += length;
                type = "Framebuffer";
                break;
            default:
                memory->unknown += length;
                type = "Unknown";
        }
        printf("Base: 0x%09x, Length: 0x%09x, Type: %s", memory_map_request.response->entries[i]->base, length, type);
    }
}

uint64_t getCR2()
{
	uint64_t val;
	__asm__ volatile ( "mov %%cr2, %0" : "=r"(val) );
    return val;
}