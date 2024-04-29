/**
 * @file linkedlist.h
 * @author GAMINGNOOBdev (https://github.com/GAMINGNOOBdev)
 * @brief Contains a really basic implementation of a linked list
 * @version 0.1
 * @date 2023-12-20
 * 
 * @copyright Copyright (c) Pradosh & GAMINGNOOBdev 2023
 */
#ifndef __LINKEDLIST_H_
#define __LINKEDLIST_H_

#include <stdint.h>
#include <stdbool.h>

struct list_node
{
    struct list_node *prev;
    void* data;
    struct list_node *next;
};

typedef struct
{
    struct list_node *start;
    struct list_node *end;
    uint64_t size;
} list;

/**
 * @brief Initializes a list
 * 
 * @param obj The new list object
 */
void list_init(list *obj);

/**
 * @brief Clears a lists ontents
 * 
 * @param obj The list that'll be cleared
 */
void list_clear(list *obj);

/**
 * @brief Checks if the list is empty
 * 
 * @param obj The list which will be checked
 * 
 * @returns `true` if the list is empty, otherwise `false`
 */
bool list_empty(list *obj);

/**
 * @brief Adds an object to the list
 * 
 * @param obj The list that'll be used
 * @param data The element that'll be added to the list
 */
void list_push_back(list *obj, void* data);

/**
 * @brief Removes the last element from the list and returns it's value
 * 
 * @param obj The list that'll be used
 * 
 * @returns The stored element
 */
void* list_pop_back(list *obj);

/**
 * @brief Gets a node of a list
 * 
 * @param obj The list object
 * @param index Index of the node
 * 
 * @returns The node, `NULL` if not in range
 */
struct list_node* list_at(list *obj, int index);

/**
 * @brief Gets the contents of a list
 * 
 * @param obj The list object
 * @param index Index of the element
 * 
 * @returns The stored element, `NULL` if not in range
 */
void* list_get(list *obj, int index);

#endif