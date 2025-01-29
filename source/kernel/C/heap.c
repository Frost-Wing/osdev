#include <heap.h>

#define PAGE_SIZE 4096 // Adjust as needed

// Simplified memory management using a single linked list
typedef struct Node {
    void* data;
    size_t size;
    struct Node* next;
} Node;

Node* head = NULL;

void* sbrk(int increment) {
    static void* current_brk = NULL; // Current break point

    if (current_brk == NULL) {
        // Get initial break point 
        current_brk = (void*)0x10000000; // Example initial break point 
    }

    // Update break point
    void* new_brk = (void*)((char*)current_brk + increment);

    // Check for address space overflow (replace with actual checks)
    if ((unsigned long)new_brk < (unsigned long)current_brk) {
        return (void*)-1; // Allocation failed
    }

    current_brk = new_brk;
    return current_brk;
}

void* malloc(size_t size) {
    // Align allocation to page size
    size_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    Node* current = head;
    Node* prev = NULL;

    // Find a suitable existing block
    while (current != NULL && current->size < aligned_size) {
        prev = current;
        current = current->next;
    }

    if (current != NULL && current->size == aligned_size) {
        // Exact match found
        if (prev) {
            prev->next = current->next;
        } else {
            head = current->next;
        }
        return current->data;
    }

    // Allocate a new block
    void* ptr = sbrk(aligned_size); 
    if (ptr == (void*)-1) {
        return NULL; // Allocation failed
    }

    Node* new_node = (Node*)ptr;
    new_node->data = ptr;
    new_node->size = aligned_size;
    new_node->next = head;
    head = new_node;

    return ptr;
}

void free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    Node* current = head;
    Node* prev = NULL;

    while (current != NULL && current->data != ptr) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        return; // Invalid pointer
    }

    if (prev) {
        prev->next = current->next;
    } else {
        head = current->next;
    }

    // sbrk(0) - current->size; // Simplified deallocation (may not be entirely accurate)
}

void* realloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size); // Use your malloc
    }

    if (size == 0) {
        free(ptr); // Use your free
        return NULL;
    }

    Node* current = head;
    while (current != NULL && current->data != ptr) {
        current = current->next;
    }

    if (current == NULL) {
        return NULL; // Invalid pointer
    }

    if (current->size == size) {
        return ptr; // No change needed
    }

    // Try to expand in place (more efficient)
    // ... (Implementation for expanding in place, if possible) ...

    // If expanding in place isn't possible, allocate a new block
    void* new_ptr = malloc(size); // Use your malloc
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_size = (size < current->size) ? size : current->size; // Don't copy more than the smaller size
    memcpy(new_ptr, ptr, copy_size);
    free(ptr); // Use your free

    current->data = new_ptr;
    current->size = size; // Update the size!
    return new_ptr;
}