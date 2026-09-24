#ifndef SMP_H
#define SMP_H

#include <basics.h>
#include <limine.h>
#include <stdbool.h>

#define SMP_MAX_CPUS 256

typedef void (*smp_call_fn_t)(void *context);

/* Start Limine-provided APs and wait until all of them are ready for work. */
bool smp_init(struct limine_smp_response *response);

/* Run fn on an AP identified by its Limine CPU array index and wait for it. */
bool smp_call(uint32_t cpu_index, smp_call_fn_t fn, void *context);

/* Run fn once on every online AP and wait for all calls to complete. */
bool smp_call_all(smp_call_fn_t fn, void *context);

uint32_t smp_cpu_count(void);
bool smp_cpu_is_online(uint32_t cpu_index);

#endif
