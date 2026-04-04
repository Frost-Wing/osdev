/**
 * @file helper.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Contains helper code for syscalls.
 * @version 0.1
 * @date 2026-04-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef SYS_HELPER_H
#define SYS_HELPER_H

#include <heap.h>
#include <syscalls.h>
#include <memory.h>

/**
 * @brief Frees a dynamically allocated array of strings.
 *
 * Releases memory for each string in the array and then frees
 * the array itself. Assumes each element and the array were
 * allocated using the kernel allocator (kfree-compatible).
 *
 * @param arr   Pointer to the array of string pointers.
 * @param count Number of elements in the array.
 *
 * @note If @p arr is NULL, the function does nothing.
 * @warning Each element in @p arr must be individually allocated.
 */
static void free_copied_string_array(char** arr, int count) {
    if (!arr)
        return;

    for (int i = 0; i < count; ++i) {
        if (arr[i])   // safety check
            kfree(arr[i]);
    }

    kfree(arr);
}

/**
 * @brief Copies a NULL-terminated array of user strings into kernel space.
 *
 * Allocates a new array of strings and copies each string from the user-provided
 * array into kernel-managed memory. The resulting array is always NULL-terminated.
 *
 * A maximum of 32 strings are copied to prevent unbounded memory usage.
 *
 * @param user_arr Pointer to a NULL-terminated array of user-space strings.
 * @param out_arr  Output pointer that receives the newly allocated array.
 *
 * @return Number of strings successfully copied on success.
 * @retval 0 If @p user_arr is NULL (no strings to copy).
 * @retval -LINUX_ENOMEM If memory allocation fails at any point.
 *
 * @note The caller is responsible for freeing the returned array using
 *       free_copied_string_array().
 *
 * @warning Each string in @p user_arr must be valid and accessible.
 *          No validation of user-space pointers is performed.
 *
 * @warning The function copies at most 32 strings; additional entries
 *          in @p user_arr are ignored.
 */
static int copy_user_string_array(char* const* user_arr, char*** out_arr) {
    if (!user_arr) {
        *out_arr = NULL;
        return 0;
    }

    char** copied = kmalloc(sizeof(char*) * 33);
    if (!copied)
        return -LINUX_ENOMEM;

    int count = 0;
    for (; count < 32; ++count) {
        const char* src = user_arr[count];
        if (!src) {
            copied[count] = NULL;
            *out_arr = copied;
            return count;
        }

        size_t len = strlen(src);
        copied[count] = kmalloc(len + 1);
        if (!copied[count]) {
            free_copied_string_array(copied, count);
            return -LINUX_ENOMEM;
        }

        memcpy(copied[count], src, len + 1);
    }

    copied[32] = NULL;
    *out_arr = copied;
    return 32;
}

/**
 * @brief Computes a hash value for a filesystem path.
 *
 * Uses a 32-bit FNV-1a hash algorithm to generate a deterministic
 * hash from a null-terminated path string. This can be used for
 * fast lookup or mapping paths to inode identifiers.
 *
 * @param path Null-terminated string representing the file path.
 *
 * @return 32-bit hash value for the given path.
 *
 * @note Returns a non-zero hash value. If the computed hash is zero,
 *       it is replaced with 1 to avoid special-case handling.
 *
 * @warning The input @p path must be a valid null-terminated string.
 *          Passing NULL results in undefined behavior.
 */
static uint32_t path_inode_hash(const char* path) {
    if (!path)
        return 1;

    uint32_t hash = 2166136261u;
    for (const unsigned char* p = (const unsigned char*)path; p && *p; ++p) {
        hash ^= *p;
        hash *= 16777619u;
    }
    return hash ? hash : 1;
}

#endif