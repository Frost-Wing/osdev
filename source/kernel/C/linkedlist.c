#include <linkedlist.h>
#include <stddef.h>
#include <heap.h>

/**
 * @brief Creates a new list node
 * 
 * @param data Element data
 * @param prev Previous node pointer
 * @param next Next node pointer
 * @returns A new node object
 */
struct list_node* list_create_node(void* data, struct list_node* prev, struct list_node* next)
{
    if (data == NULL)
        return NULL;

    struct list_node *node = (struct list_node*)kmalloc(sizeof(struct list_node));

    node->data = data;
    node->prev = prev;
    node->next = next;

    return node;
}

/**
 * @brief Deletes a list node
 * 
 * @param node The node that will be deleted
 */
void list_delete_node(struct list_node* node)
{
    if (node == NULL)
        return;

    kfree(node);
}

/**
 * @brief Initializes a list
 * 
 * @param obj The new list object
 */
void list_init(list *obj)
{
    if (obj == NULL)
        return;

    (*obj).size = 0;
    (*obj).start = NULL;
    (*obj).end = NULL;
}

/**
 * @brief Clears a lists ontents
 * 
 * @param obj The list that'll be cleared
 */
void list_clear(list *obj)
{
    if (obj == NULL)
        return;

    if (list_empty(obj))
        return;

    struct list_node* currentNode = obj->start;
    while (currentNode != NULL)
    {
        struct list_node* next = currentNode->next;
        list_delete_node(currentNode);
        currentNode = next;
    }
    
    list_init(obj);
}

/**
 * @brief Checks if the list is empty
 * 
 * @param obj The list which will be checked
 * 
 * @returns `true` if the list is empty, otherwise `false`
 */
bool list_empty(list *obj)
{
    if (obj == NULL)
        return true;

    if (obj->start == obj->end == NULL || obj->size == 0)
        return true;

    return false;
}

/**
 * @brief Adds an object to the list
 * 
 * @param obj The list that'll be used
 * @param data The element that'll be added to the list
 */
void list_push_back(list *obj, void* data)
{
    if (obj == NULL || data == NULL)
        return;

    struct list_node* node = list_create_node(data, NULL, NULL);

    if (list_empty(obj))
    {
        obj->start = node;
        obj->end = node;
        obj->size++;
        return;
    }

    node->prev = obj->end;
    obj->end->next = node;
    obj->end = node;
    obj->size++;
}

/**
 * @brief Removes the last element from the list and returns it's value
 * 
 * @param obj The list that'll be used
 * 
 * @returns The stored element
 */
void* list_pop_back(list *obj)
{
    if (obj == NULL)
        return NULL;

    if (list_empty(obj))
        return NULL;

    struct list_node *node = obj->end;
    struct list_node *end = node->prev;
    obj->end = end;

    void* data = node->data;
    list_delete_node(node);

    obj->size--;

    return data;
}

/**
 * @brief Gets a node of a list
 * 
 * @param obj The list object
 * @param index Index of the node
 * 
 * @returns The node, `NULL` if not in range
 */
struct list_node* list_at(list *obj, int index)
{
    if (obj == NULL)
        return NULL;

    if (list_empty(obj))
        return NULL;

    if (index < 0 || index >= obj->size)
        return NULL;
    
    if (index == 0)
        return obj->start->data;

    if (index == obj->size-1)
        return obj->end->data;

    struct list_node *currentNode = obj->start;
    for (int i = 0; i < index; i++)
    {
        if (currentNode->next == NULL)
            break;

        currentNode = currentNode->next;
    }

    return currentNode;
}

/**
 * @brief Gets the contents of a list
 * 
 * @param obj The list object
 * @param index Index of the element
 * 
 * @returns The stored element, `NULL` if not in range
 */
void* list_get(list *obj, int index)
{
    struct list_node *node = list_at(obj, index);

    if (node == NULL)
        return NULL;

    return node->data;
}