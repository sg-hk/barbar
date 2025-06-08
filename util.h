#ifndef UTIL_H
#define UTIL_H

#include <semaphore.h>
#include <pthread.h>

#include "config.h"

/* const int won't be accepted by compiler */
#define NUM_MODULES (sizeof(MODULES) / sizeof(MODULES[0]))

/* the struct used by consumer and producers for IPC */
struct shared_data {
	pthread_mutex_t mutex;
	pthread_cond_t  cond;
	unsigned long 	version; /* allows simple check for new data */
	char            slots[NUM_MODULES][MSG_LEN];
};

/* there is never any need to change this */
static const char SHM_NAME[] = "/shm_barbar";

/* forward declarations of shared functions */
void log_err(const char *fmt, ...);
void w2s(const char *module_name, const char *fmt, ...);

#endif
