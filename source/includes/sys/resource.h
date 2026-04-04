/**
 * @file resource.h
 * @author Pradosh (pradoshgame@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2026-04-03
 * 
 * @copyright Copyright (c) Pradosh 2026
 * 
 */
#ifndef SYS_RESOURCE_H
#define SYS_RESOURCE_H

#include <stdint.h>

typedef struct {
    uint64_t rlim_cur;
    uint64_t rlim_max;
} linux_rlimit64_t;

#endif