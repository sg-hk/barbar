#include <X11/Xlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>

#include "config.h"

volatile sig_atomic_t stop_flag = 0;
void handle_signal(int sig) {
        (void)sig;  // suppress unused warning
        stop_flag = 1;
}

int main(void) {
        /* final output string */
        char *out_str = malloc(MAX_READ);
        if (!out_str) { perror("malloc out_str"); exit(1); }

        /* shared mem object */
        size_t shm_size = NUM_MODULES * MSG_SIZE;

        int shm_fd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (shm_fd == -1) { perror("shm_open"); exit(1); }
        if (ftruncate(shm_fd, shm_size) == -1) { perror("ftruncate"); exit(1); }

        char *shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED) { perror("mmap"); exit(1); }

        /* semaphore */
        sem_t *sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0600, 1);
        if (sem == SEM_FAILED) { perror("sem_open"); exit(1); }

        /* zero out shared memory slots */
        sem_wait(sem);
        memset(shm_ptr, 0, shm_size);
        sem_post(sem);

        /* X display */
        Display *display = XOpenDisplay(NULL);
        if (display == NULL) { fprintf(stderr, "Cannot open display\n"); exit(1); }
        Window root = DefaultRootWindow(display);

        /* signal handling for CTRL+C, CTRL+D */
        struct sigaction sa;
        sa.sa_handler = handle_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        /* main loop: read shared memory, concat messages, update bar */
        while (!stop_flag) {
                sem_wait(sem);
                out_str[0] = '\0';
                int cur_len = 0;
                for (int i = 0; i < NUM_MODULES; i++) {
                        /* calculate the pointer to slot i */
                        char *slot = shm_ptr + (i * MSG_SIZE);
                        if (slot[0] == '\0')
                                continue;
                        if (cur_len > 0) {
                                strncat(out_str, SEP, MAX_READ - cur_len);
                                cur_len = strlen(out_str);
                        }
                        strncat(out_str, slot, MAX_READ - cur_len);
                        cur_len = strlen(out_str);
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
        if (munmap(shm_ptr, shm_size) == -1) { perror("munmap"); }
        close(shm_fd);
        if (shm_unlink(SHM_NAME) == -1) { perror("shm_unlink"); }
        if (sem_close(sem) == -1) { perror("sem_close"); }
        if (sem_unlink(SEM_NAME) == -1) { perror("sem_unlink"); }
        XCloseDisplay(display);
        return 0;
}
