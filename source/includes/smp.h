/**
 * @file smp.h
 * @brief Symmetric multiprocessing initialization and call interfaces.
 */
#ifndef SMP_H
#define SMP_H

#include <basics.h>
#include <limine.h>
#include <stdbool.h>

#define SMP_MAX_CPUS 256

typedef void (*smp_call_fn_t)(void *context);

/**
 * @brief Start Limine-provided APs and wait until they are ready for work.
 * @param response Limine SMP response.
 * @return true if initialization succeeds.
 */
bool smp_init(struct limine_smp_response *response);

/**
 * @brief Run a callback on an AP and wait for it to finish.
 * @param cpu_index Index in Limine's CPU array.
 * @param fn Callback to execute.
 * @param context Opaque value passed to the callback.
 * @return true if the callback was dispatched successfully.
 */
bool smp_call(uint32_t cpu_index, smp_call_fn_t fn, void *context);

/**
 * @brief Run a callback on every online AP and wait for completion.
 * @param fn Callback to execute.
 * @param context Opaque value passed to the callback.
 * @return true if all callbacks were dispatched successfully.
 */
bool smp_call_all(smp_call_fn_t fn, void *context);

/** @brief Start the sample terminal cursor blinker on an online AP. */
bool smp_start_cursor_blink(void);

/** @brief Return the number of online CPUs. */
uint32_t smp_cpu_count(void);

/** @brief Check whether a CPU is online. */
bool smp_cpu_is_online(uint32_t cpu_index);

#endif
