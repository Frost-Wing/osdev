/**
 * @file namespace.c
 * @brief Generic flat-array "path namespace" engine shared by procfs and sysfs.
 *
 * See namespace.h for the rationale. This is the traversal logic that used
 * to be duplicated (with small, silent divergences) between procfs and
 * sysfs in proc.c - now it lives in exactly one place.
 */
#include <filesystems/layers/namespace.h>
#include <strings.h>
#include <memory.h>
#include <sort.h>

void ns_normalize_path(const char *in, char *out, size_t out_sz) {
    if (!in || out_sz == 0) {
        if (out_sz) out[0] = '\0';
        return;
    }

    size_t len = strlen(in);
    if (len >= out_sz)
        len = out_sz - 1;

    memcpy(out, in, len);
    out[len] = '\0';

    while (len > 0 && out[len - 1] == '/')
        out[--len] = '\0';
}

int ns_reply(vfs_file_t *file, uint8_t *buf, uint32_t size, const char *data, int len) {
    if (file->pos >= (uint32_t)len)
        return 0;

    uint32_t rem = len - file->pos;
    if (rem > size)
        rem = size;

    memcpy(buf, data + file->pos, rem);
    file->pos += rem;
    return rem;
}

procfs_entry_t *ns_find(procfs_entry_t **entries, int count, const char *path) {
    if (!path)
        return NULL;

    char normalized[256];
    ns_normalize_path(path, normalized, sizeof(normalized));

    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i]->name, normalized) == 0)
            return entries[i];
    }
    return NULL;
}

/* Is `name` (an entry's full registered name) a descendant of `prefix`?
 * If so, point *child at its direct-child path component (the bit right
 * after the prefix, up to the next '/' or the end of the string) and
 * report whether that component is the whole rest of the name (a leaf)
 * or has further nesting beneath it (implying a directory). */
static bool ns_child_of(const char *name, const char *prefix, size_t prefix_len,
                         const char **child, size_t *child_len, bool *is_leaf) {
    if (prefix_len) {
        if (strncmp(name, prefix, prefix_len) != 0)
            return false;
        name += prefix_len;

        if (*name == '/')
            name++;
        else if (*name != '\0')
            return false; /* shares a prefix but at a component boundary mismatch */
    }

    if (*name == '\0')
        return false; /* `name` *is* `prefix` - not a child of itself */

    const char *slash = strchr(name, '/');
    *child = name;
    *child_len = slash ? (size_t)(slash - name) : strlen(name);
    *is_leaf = (slash == NULL);
    return true;
}

/* Directories first, then files; alphabetical within each group. */
static int ns_child_cmp(const void *a, const void *b) {
    const ns_child_t *x = (const ns_child_t *)a;
    const ns_child_t *y = (const ns_child_t *)b;

    if (x->is_dir != y->is_dir)
        return x->is_dir ? -1 : 1;

    size_t min = x->len < y->len ? x->len : y->len;
    int r = strncmp(x->name, y->name, min);
    if (r != 0)
        return r;

    if (x->len == y->len)
        return 0;
    return x->len < y->len ? -1 : 1;
}

int ns_getdent(procfs_entry_t **entries, int count, const char *path,
               uint64_t index, const char **out_name, procfs_type_t *out_type) {
    if (!path || !out_name || !out_type)
        return 0;

    char normalized[256];
    ns_normalize_path(path, normalized, sizeof(normalized));
    size_t plen = strlen(normalized);

    /* Static like dent_name below - not reentrant, keeps the kernel stack small. */
    static ns_child_t children[NS_MAX_CHILDREN];
    size_t n = 0;

    /* 1) Collect unique direct children. */
    for (int i = 0; i < count; i++) {
        const char *child;
        size_t child_len;
        bool is_leaf;

        if (!ns_child_of(entries[i]->name, normalized, plen, &child, &child_len, &is_leaf))
            continue;

        /* It's a directory if it has anything nested beneath it, or the
         * entry itself is registered as a directory. */
        bool dir = !is_leaf || entries[i]->type == PROC_DIR;

        bool found = false;
        for (size_t j = 0; j < n; j++) {
            if (children[j].len == child_len &&
                strncmp(children[j].name, child, child_len) == 0) {
                /* Same name seen before: if any occurrence is a dir, it's a dir. */
                if (dir) {
                    children[j].is_dir = true;
                    children[j].type = PROC_DIR;
                }
                found = true;
                break;
            }
        }
        if (found)
            continue;

        if (n >= NS_MAX_CHILDREN)
            break;

        children[n].name   = child;
        children[n].len    = child_len;
        children[n].is_dir = dir;
        children[n].type   = dir ? PROC_DIR : entries[i]->type;
        n++;
    }

    if (index >= n)
        return 0;

    /* 2) Sort: dirs first, then files, alphabetical within each. */
    qsort(children, n, sizeof(ns_child_t), ns_child_cmp);

    /* 3) Hand back the requested one. */
    static char dent_name[64];
    const ns_child_t *c = &children[index];
    size_t len = (c->len >= sizeof(dent_name)) ? sizeof(dent_name) - 1 : c->len;
    memcpy(dent_name, c->name, len);
    dent_name[len] = '\0';

    *out_name = dent_name;
    *out_type = c->type;
    return 1;
}

int ns_ls(procfs_entry_t **entries, int count, const char *path) {
    uint64_t i = 0;
    const char *name;
    procfs_type_t type;

    while (ns_getdent(entries, count, path, i++, &name, &type)) {
        printfnoln(
            type == PROC_DIR ? blue_color "%s/ " reset_color : green_color "%s " reset_color,
            name);
    }
    return 0;
}

int ns_is_dir(procfs_entry_t **entries, int count, const char *path) {
    if (!path)
        return -1;

    char normalized[256];
    ns_normalize_path(path, normalized, sizeof(normalized));

    if (normalized[0] == '\0')
        return 1; /* the namespace root is always a directory */

    procfs_entry_t *e = ns_find(entries, count, normalized);
    if (e)
        return e->type == PROC_DIR ? 1 : 0;

    /* No exact entry: it's still a directory if something lives beneath it. */
    size_t plen = strlen(normalized);
    for (int i = 0; i < count; i++) {
        const char *child;
        size_t child_len;
        bool is_leaf;
        if (ns_child_of(entries[i]->name, normalized, plen, &child, &child_len, &is_leaf))
            return 1;
    }
    return -1;
}