/**
 * @file paging.c
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief The source for Paging
 * @version 0.1
 * @date 2023-12-17
 * 
 * @copyright Copyright (c) Pradosh 2023
 * 
 */
#include <paging.h>

int8 page_bitmap[amount_of_pages / 8];

void initialize_page_bitmap() {
    info("Initializing paging!", __FILE__);
    for (size_t i = 0; i < sizeof(page_bitmap) / sizeof(page_bitmap[0]); ++i) {
        page_bitmap[i] = 0;
    }
    done("Successfully initialized page bitmap!", __FILE__);
}

void* allocate_page() {
    for (size_t i = 0; i < amount_of_pages; ++i) {
        size_t byte_offset = i / 8;
        size_t bit_offset = i % 8;

        if (!(page_bitmap[byte_offset] & (1 << bit_offset))) {
            page_bitmap[byte_offset] |= (1 << bit_offset);

            return (void*)(memory_start + i * page_size);
        }
    }

    error("No free pages available!", __FILE__);
    return NULL;
}

void free_page(void* addr) {
    size_t page_index = ((int64)addr - memory_start) / page_size;

    size_t byte_offset = page_index / 8;
    size_t bit_offset = page_index % 8;

    page_bitmap[byte_offset] &= ~(1 << bit_offset);
}