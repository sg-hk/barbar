#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "config.h"

static int xsem_wait(sem_t *s);
static int xsem_post(sem_t *s);
static void handle_signal(int sig);

static volatile sig_atomic_t terminate;

int
main(void)
{
#ifdef __OPENBSD__
        if (pledge("stdio rpath wpath cpath fattr id proc exec shm unveil", NULL) == -1) {
                perror("pledge");
                exit(1);
        }
#endif

	struct timespec ts;
        size_t shm_size = NUM_MODULES * MSG_SIZE;
        bool locked = false;
	terminate = 0;

        int shm_fd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (shm_fd == -1 && errno == EEXIST)
                shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
        if (shm_fd == -1) {
                fprintf(stderr, "shm_open\n");
		return 0;
        }

        if (ftruncate(shm_fd, shm_size) == -1) {
                fprintf(stderr, "ftruncate\n");
                goto cleanup_shm_fd;
        }

        char *shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED) {
                fprintf(stderr, "mmap\n");
                goto cleanup_shm_fd;
        }

        sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0600, 1);
        if (sem == SEM_FAILED) {
                fprintf(stderr, "sem_open\n");
                goto cleanup_mmap;
        }

#ifdef __OPENBSD__
        unveil("/dev/shm", "rw");
        unveil(NULL, NULL);                 
	/* tighten pledge */
        if (pledge("stdio shm", NULL) == -1) {
                perror("second pledge");
                goto cleanup_sem;
        }
#endif

        /* zero shared memory on first start */
        if (xsem_wait(sem) == -1) {
                fprintf(stderr, "sem_wait\n");
                goto cleanup_sem;
        }
        memset(shm_ptr, 0, shm_size);
        xsem_post(sem);

        struct sigaction sa = {0};
        sa.sa_handler = handle_signal;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT,  &sa, NULL) == -1 ||
            sigaction(SIGTERM, &sa, NULL) == -1) {
                perror("sigaction");
                goto cleanup_sem;
        }

        /* main loop */
        char out_str[MAX_READ];

        while (!terminate) {
                if (xsem_wait(sem) == -1) {
                        fprintf(stderr, "sem_wait\n");
                        break;
                }
                locked = true;

                out_str[0] = '\0';
                int cur_len = 0;
                const int sep_len = strlen(SEP);

                for (int i = 0; i < NUM_MODULES; ++i) {
                        char *slot = shm_ptr + (i * MSG_SIZE);
                        if (slot[0] == '\0')
                                continue;

                        size_t slot_len = strlen(slot);
                        size_t need = slot_len + (cur_len ? sep_len : 0);

                        /* overflow check – BEFORE write */
                        if (cur_len + (int)need >= MAX_READ) {
                                xsem_post(sem);
                                locked = false;
                                fprintf(stderr, "string too long\n");
                                goto sleep_only; 
                        }

                        if (cur_len) {
                                strncat(out_str, SEP, MAX_READ - cur_len - 1);
                                cur_len += sep_len;
                        }
                        strncat(out_str, slot, MAX_READ - cur_len - 1);
                        cur_len += slot_len;
                }

                xsem_post(sem);
                locked = false;

		/* EVERYTHING BUILDS UP TO THIS MOMENT ! */
                if (out_str[0] != '\0') {
                        printf("%s\n", out_str);
			fflush(stdout);
		}

sleep_only:
		ts.tv_sec = RFRSH_SEC;
		ts.tv_nsec = RFRSH_NSEC;
		while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
			continue; /* finish remaining time */
        }

        printf("barbar was terminated\n");

	/* clean ups */
cleanup_sem:
        if (locked) xsem_post(sem);
        if (sem_close(sem) == -1)
                fprintf(stderr, "sem_close\n");
        if (sem_unlink(SEM_NAME) == -1)
                fprintf(stderr, "sem_unlink\n");

cleanup_mmap:
        if (munmap(shm_ptr, shm_size) == -1)
                fprintf(stderr, "munmap\n");

cleanup_shm_fd:
        close(shm_fd);
        if (shm_unlink(SHM_NAME) == -1)
                fprintf(stderr, "shm_unlink\n");

        return 0;
}

/* helper functions */
/* sem_wait() that retries on EINTR */
static int
xsem_wait(sem_t *s)
{
        int r;
        do {
                r = sem_wait(s);
        } while (r == -1 && errno == EINTR);
        return r;
}

/* same for sem_post() */
static int
xsem_post(sem_t *s)
{
        int r;
        do {
                r = sem_post(s);
        } while (r == -1 && errno == EINTR);
        return r;
}

static void
handle_signal(int sig)
{
        (void)sig;
        terminate = 1;
}
