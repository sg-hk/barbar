#define _GNU_SOURCE

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <syslog.h>

#include "config.h"
#include "util.h"

/* writes formatted string to syslog and exits */
void
log_err(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsyslog(LOG_ERR, fmt, args);
	va_end(args);
	exit(1);
}

/* locks mutex, writes to shared memory, unlocks mutex */
void
w2s(const char *module_name, const char *fmt, ...)
{
	/* initialize once, keep value for future calls ("static") */
	static struct shared_data *shm_data = NULL;
	static int idx = -1;

	/* on first write, initialize variables */
	if (!shm_data) {
		int shm_fd = shm_open(SHM_NAME, O_RDWR, 0600);
		if (shm_fd == -1)
			log_err("%s: shm_open failed", module_name);
		shm_data = mmap(NULL, sizeof(struct shared_data),
		                PROT_READ | PROT_WRITE, 
				MAP_SHARED, shm_fd, 0);
		if (shm_data == MAP_FAILED)
			log_err("%s: mmap failed", module_name);

		/* find the index based on module name */
		for (int i = 0; i < (int)NUM_MODULES; ++i) {
			if (strcmp(MODULES[i], module_name) == 0) {
				idx = i;
				break;
			}
		}
		if (idx == -1)
			log_err("Module name \"%s\" not found", module_name);
	}

	char buf[MSG_LEN];
	va_list args;
	va_start(args, fmt);
	/* this automatically cuts off at MSG_LEN */
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	pthread_mutex_lock(&shm_data->mutex);
	++shm_data->version;
	snprintf(shm_data->slots[idx], MSG_LEN, "%s", buf);
	pthread_cond_broadcast(&shm_data->cond);
	pthread_mutex_unlock(&shm_data->mutex);
}
