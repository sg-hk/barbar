#include <X11/Xlib.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <syslog.h>
#include <stdbool.h>
#include <unistd.h>

#include "config.h"
#include "util.h"

/* sets status bar to error message and exits */
void handle_signal(int);

static bool terminate;

int 
main(void)
{
        /* X display */
        Display *display = XOpenDisplay(NULL);
        if (display == NULL) 
                log_err("XOpenDisplay");
        Window root = DefaultRootWindow(display);

        /* final output string */
        char *out_str = calloc(MAX_READ, 1);
        if (!out_str)
                log_err("calloc out_str");

        /* signal handling for CTRL+C (sigint), kill (sigterm) */
        struct sigaction sa;
        sa.sa_handler = handle_signal; // signal handler logs and exits
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        /* shared memory object and semaphor set up */
        size_t shm_size = NUM_MODULES * MSG_SIZE;
        int shm_fd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (shm_fd == -1) 
                log_err("shm_open");
        if (ftruncate(shm_fd, shm_size) == -1) 
                log_err("ftruncate");

        char *shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED)
                log_err("mmap");

        sem_t *sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0600, 1);
        if (sem == SEM_FAILED)
                log_err("sem_open");

        /* zero out shared memory region */
        sem_wait(sem);
        memset(shm_ptr, 0, shm_size);
        sem_post(sem);

        /* main loop: read shared memory, concat messages, update bar */
        while (!terminate) {
                sem_wait(sem);
                out_str[0] = '\0';
                int cur_len = 0;
                int sep_len = strlen(SEP);
                for (int i = 0; i < NUM_MODULES; ++i) {
                        /* 
                         * calculate the pointer to slot i 
                         * assume each i-th slot is null-terminated
                         */
                        char *slot = shm_ptr + (i * MSG_SIZE);
                        if (slot[0] == '\0')
                                continue;
                        if (cur_len > 0) {
                                strncat(out_str, SEP, MAX_READ - cur_len - 1);
                                cur_len += sep_len;
                        }
                        strncat(out_str, slot, MAX_READ - cur_len - 1);
                        cur_len += strlen(slot);
                }
                sem_post(sem);

                if (out_str[0] != '\0') {
                        /* update window root = dwm bar */
                        XStoreName(display, root, out_str);
                        XSync(display, False);
                }

                usleep((useconds_t)(RFRSH * 1000000));
        }

        /* clean up triggered by signal */
        XStoreName(display, root, "barbar was terminated");
        XSync(display, False);
        free(out_str);
        if (munmap(shm_ptr, shm_size) == -1)
                log_err("munmap");
        close(shm_fd);
        if (shm_unlink(SHM_NAME) == -1)
                log_err("shm_unlink");
        if (sem_close(sem) == -1)
                log_err("sem_close");
        if (sem_unlink(SEM_NAME) == -1)
                log_err("sem_unlink");
        XCloseDisplay(display);
        return 0;
}

void 
handle_signal(int sig)
{
        (void)sig;  // suppress unused warning
        terminate = true;
}
