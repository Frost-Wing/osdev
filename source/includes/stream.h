/**
 * @file stream.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief Unix like standard streams.
 * @version 0.1
 * @date 2026-01-03
 *
 * @copyright Copyright (c) Pradosh 2026
 *
 */
#ifndef STREAM_H
#define STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct vfs_file vfs_file_t;

typedef enum {
    STDIN = 0,
    STDOUT = 1,
    STDERR = 2
} stream_t;

#define STREAM_MAX_FDS 256
#define FD_PIPE_WOULD_BLOCK (-2)

/** @brief Initialize the standard streams. */
void stream_init(void);

/**
 * @brief Redirect output to a file.
 *
 * @param s the stream type.
 * @param file If file is 'null' then output will be in terminal.
 *
 * @returns File descriptor of the given file.
 */
int stream_set_file(stream_t s, vfs_file_t* file);

/**
 * @brief Get the file currently backing a standard stream.
 * @param s Stream to query.
 * @return Its file, or NULL if it is not file-backed.
 */
vfs_file_t* stream_get_file(stream_t s);

/**
 * @brief Write bytes to a standard stream.
 * @param s Destination stream.
 * @param buf Data to write.
 * @param len Number of bytes to write.
 */
void stream_write(stream_t s, const char *buf, size_t len);

/**
 * @brief Write one character to a standard stream.
 * @param s Destination stream.
 * @param c Character to write.
 */
void stream_putc(stream_t s, char c);

/** @brief Initialize the process file-descriptor table. */
void fd_table_init(void);

/** @brief Check whether a file descriptor refers to an open entry. */
bool fd_valid(int fd);

/** @brief Check whether a descriptor refers to a terminal. */
bool fd_is_tty(int fd);

/** @brief Get the VFS file associated with a descriptor. */
vfs_file_t *fd_get_file(int fd);

/**
 * @brief Open a path and allocate a file descriptor.
 * @param path Path to open.
 * @param flags Open flags.
 * @return Descriptor on success, or a negative error code.
 */
int fd_open(const char *path, int flags);

/**
 * @brief Create a virtual descriptor for a path.
 * @param path Virtual path associated with the descriptor.
 * @param flags Descriptor flags.
 * @return Descriptor on success, or a negative error code.
 */
int fd_create_virtual(const char *path, int flags);

/** @brief Close a descriptor and release its resources. */
int fd_close(int fd);

/** @brief Duplicate a descriptor into the next available slot. */
int fd_dup(int oldfd);

/** @brief Duplicate a descriptor into a specified slot. */
int fd_dup2(int oldfd, int newfd);

/** @brief Return the flags associated with a descriptor. */
int fd_flags(int fd);

/** @brief Return the path associated with a descriptor, if any. */
const char *fd_get_path(int fd);

/** @brief Return the size of a descriptor's file in bytes. */
uint32_t fd_file_size(int fd);

/** @brief Return a pointer to a descriptor's current file position. */
uint32_t *fd_pos_ptr(int fd);

/**
 * @brief Create a pipe and return its read and write descriptors.
 * @param fds Receives the read descriptor at index 0 and write descriptor at index 1.
 * @param nonblocking Whether pipe operations should be nonblocking.
 * @return 0 on success, or a negative error code.
 */
int fd_pipe_create(int fds[2], bool nonblocking);

/** @brief Check whether a descriptor refers to a pipe. */
bool fd_is_pipe(int fd);

/** @brief Check whether a pipe has data available for reading. */
int fd_pipe_poll(int fd);

/**
 * @brief Read bytes from a pipe.
 * @param fd Pipe descriptor.
 * @param buf Destination buffer.
 * @param count Maximum number of bytes to read.
 * @return Bytes read, or a negative error code.
 */
int fd_pipe_read(int fd, void *buf, size_t count);

/**
 * @brief Write bytes to a pipe.
 * @param fd Pipe descriptor.
 * @param buf Source buffer.
 * @param count Number of bytes to write.
 * @return Bytes written, or a negative error code.
 */
int fd_pipe_write(int fd, const void *buf, size_t count);

/**
 * @brief Create an event counter and return its descriptor.
 * @param initial_value Initial counter value.
 * @param semaphore Whether reads decrement the counter by one.
 * @param nonblocking Whether operations should be nonblocking.
 * @return Descriptor on success, or a negative error code.
 */
int fd_eventfd_create(uint64_t initial_value, bool semaphore, bool nonblocking);

/** @brief Check whether a descriptor refers to an event counter. */
bool fd_is_eventfd(int fd);

/** @brief Check whether an event counter is nonblocking. */
bool fd_eventfd_nonblocking(int fd);

/**
 * @brief Read an event counter value.
 * @param fd Event-counter descriptor.
 * @param value Receives the counter value.
 * @return 0 on success, or a negative error code.
 */
int fd_eventfd_read(int fd, uint64_t *value);

/**
 * @brief Add a value to an event counter.
 * @param fd Event-counter descriptor.
 * @param value Amount to add.
 * @return 0 on success, or a negative error code.
 */
int fd_eventfd_write(int fd, uint64_t value);

/**
 * @brief Check event-counter readiness for requested events.
 * @param fd Event-counter descriptor.
 * @param events Events to test.
 * @return Readiness bits, or a negative error code.
 */
int fd_eventfd_poll(int fd, int events);

#endif
