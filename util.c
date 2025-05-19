#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <syslog.h>

#include "config.h"
#include "util.h"

void
log_err(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsyslog(LOG_ERR, fmt, args);
    va_end(args);
    exit(1);
}

void
w2s(const char *module_name, const char *fmt, ...)
{
    static char *slot = NULL;
    static sem_t *sem = NULL;

    if (!slot) {
        int shm_fd = shm_open(SHM_NAME, O_RDWR, 0600);
        if (shm_fd == -1)
            log_err("%s: shm_open failed", module_name);

        size_t shm_size = NUM_MODULES * MSG_SIZE;
        char *shm_ptr = mmap(NULL, shm_size,
			     PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED)
            log_err("%s: mmap failed", module_name);

        sem = sem_open(SEM_NAME, 0);
        if (sem == SEM_FAILED)
            log_err("%s: sem_open failed", module_name);

        int i;
        for (i = 0; i < NUM_MODULES; ++i) {
            if (strcmp(MODULES[i], module_name) == 0)
                break;
        }
        if (i == NUM_MODULES)
            log_err("Module name \"%s\" not found", module_name);

        slot = shm_ptr + (i * MSG_SIZE);
    }

    char buf[MSG_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    sem_wait(sem);
    snprintf(slot, MSG_SIZE, "%s", buf);
    sem_post(sem);
}
