/**
 * @file namespace.h
 * @brief Generic flat-array "path namespace" engine shared by procfs and sysfs.
 *
 * Both procfs and sysfs are, structurally, the same thing: a flat array of
 * procfs_entry_t*, where directories are *implied* by other entries' names
 * sharing a "/"-prefix (e.g. registering "pci/devices" implies a "pci"
 * directory exists, without a separate entry for "pci" itself needing to
 * be a directory type). This file is that shared traversal logic, factored
 * out so any future virtual filesystem (a devfs listing, a tmpfs index,
 * etc.) can reuse it instead of re-deriving find/getdent/ls/is_dir again.
 */

#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <basics.h>
#include <filesystems/layers/proc.h>
#include <filesystems/vfs.h>

/* Normalize `in` (strip trailing slashes, truncate to out_sz) into the
 * caller-supplied `out` buffer. Unlike the old per-function `static char
 * normalized[256]` locals this replaces, this takes no ownership of any
 * shared buffer, so callers on different cores stacking their own local
 * `char buf[256]` no longer race each other here. */
void ns_normalize_path(const char *in, char *out, size_t out_sz);

/* Shared "format into tmp, then serve piecewise via file->pos" reply used
 * by every proc_*_read callback. Replaces the pos-clamping boilerplate
 * that used to be duplicated in each callback. */
int ns_reply(vfs_file_t *file, uint8_t *buf, uint32_t size, const char *data, int len);

/* Find an entry by exact (normalized) path in a flat array. NULL if absent. */
procfs_entry_t *ns_find(procfs_entry_t **entries, int count, const char *path);

/* Fill *out_name/*out_type with the `index`-th direct child of `path`
 * within the array, deduplicating entries that imply the same child
 * directory (e.g. several "bus/pci/devices/xxx" entries all implying one
 * "devices" dir). Returns 1 on success, 0 once index is past the end.
 *
 * Note: *out_name points at an internal static buffer shared across all
 * ns_getdent callers (same lifetime/reentrancy characteristics the old
 * per-function statics had - i.e. still not safe if two cores are walking
 * a directory at the exact same instant). Say the word if you want this
 * turned into a caller-supplied output buffer instead; it's a bigger
 * change because procfs_getdent/sysfs_getdent's signatures are called
 * from elsewhere in the VFS layer.
 */
int ns_getdent(procfs_entry_t **entries, int count, const char *path,
               uint64_t index, const char **out_name, procfs_type_t *out_type);

/* Print the direct children of `path`, colorized the way procfs_ls/sysfs_ls
 * always have (blue for dirs, green for files). */
int ns_ls(procfs_entry_t **entries, int count, const char *path);

/* Is `path` a directory? Root ("") always is. -1 if the path doesn't exist. */
int ns_is_dir(procfs_entry_t **entries, int count, const char *path);

#endif