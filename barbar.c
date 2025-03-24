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
#include <time.h>
#include <unistd.h>

#include "config.h"

void handle_signal(int);
void log_err(const char*, ...);

int 
main(void)
{
        /* char set for string randomization */
        const char alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz" 
                                "0123456789";

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

        /* shared mem object with randomized name for security */
        srand(time(NULL));
        size_t shm_size = NUM_MODULES * MSG_SIZE;
        int str_len = strlen(SHM_NAME) + 26; // _ + 24 chars + \0 = 26
        char* shm_rname = malloc(str_len); 
        char rstr[25];
        for (int i = 0; i < 24; ++i)
                rstr[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
        rstr[24] = '\0';
        snprintf(shm_rname, str_len, "%s_%s", SHM_NAME, rstr);

        int shm_fd = shm_open(shm_rname, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (shm_fd == -1) 
                log_err("shm_open");
        if (ftruncate(shm_fd, shm_size) == -1) 
                log_err("ftruncate");

        /* 
         * if MAP_FIXED is not set in flags, addr can be 0:
         * kernel will select address which doesn't overlap with all. memory
         * the starting address is returned in all cases
         */
        char *shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED)
                log_err("mmap");

        /* semaphore name is randomized like shm */
        char* sem_rname = malloc(str_len); 
        snprintf(sem_rname, str_len, "%s_%s", SEM_NAME, rstr);
        sem_t *sem = sem_open(sem_rname, O_CREAT | O_EXCL, 0600, 1);
        if (sem == SEM_FAILED)
                log_err("sem_open");

        /* zero out shared memory region */
        sem_wait(sem);
        memset(shm_ptr, 0, shm_size);
        sem_post(sem);

        /* main loop: read shared memory, concat messages, update bar */
        for (;;) {
                sem_wait(sem);
                out_str[0] = '\0';
                int cur_len = 0;
                int sep_len = strlen(SEP);
                for (int i = 0; i < NUM_MODULES; i++) {
                        /* calculate the pointer to slot i */
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
                        XStoreName(display, root, out_str);
                        XSync(display, False);
                }

                usleep((useconds_t)(RFRSH * 1000000));
        }

        /* clean up */
        free(out_str);
        if (munmap(shm_ptr, shm_size) == -1)
                log_err("munmap");
        close(shm_fd);
        if (shm_unlink(shm_rname) == -1)
                log_err("shm_unlink");
        if (sem_close(sem) == -1)
                log_err("sem_close");
        if (sem_unlink(sem_rname) == -1)
                log_err("sem_unlink");
        free(shm_rname);
        free(sem_rname);
        XCloseDisplay(display);
        return 0;
}
/* sets status bar to error message and exits */
void 
handle_signal(int sig)
{
        (void)sig;  // suppress unused warning

        Display *display = XOpenDisplay(NULL);
        if (display == NULL)
                log_err("could not reset status bar upon signal termination");
        Window root = DefaultRootWindow(display);
        XStoreName(display, root, "barbar was terminated");
        XSync(display, False);
        log_err("signal termination");
}

/* writes error to syslog and exits */
void 
log_err(const char *fmt, ...) 
{
        va_list args;
        va_start(args, fmt);
        vsyslog(LOG_ERR, fmt, args);
        va_end(args);
        exit(1);
}
