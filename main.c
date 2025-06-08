#define _POSIX_C_SOURCE 200809L /* strsignal */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "util.h"

static volatile sig_atomic_t terminate = 0;
static volatile sig_atomic_t term_sig = 0;

static void handle_signal(int sig);

int
main(void)
{
	struct sigaction sa = {0};
        sa.sa_handler = handle_signal;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT,  &sa, NULL) == -1 ||
            sigaction(SIGTERM, &sa, NULL) == -1)
		log_err("sigaction");

	bool is_creator = false;
	size_t shm_size = sizeof(struct shared_data);
	int shm_fd;

	/* we want to create a fresh memory region for SHM_NAME */
open_shm:
	shm_fd = shm_open(SHM_NAME, 
			O_RDWR | O_CREAT | O_EXCL, 0600);
	if (shm_fd != -1) {
		/* all good */
		is_creator = true;
	} else if (errno == EEXIST) {
		/* already exists: try to use it and continue if it works,
		 * taking into consideration we are not the creators */
		shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
		
		if (shm_fd == -1 && errno == EACCES) {
			/* permissions error from stale process:
			 * we delete and retry */
			if (shm_unlink(SHM_NAME) == -1)
				log_err("unlink stale shm");
			goto open_shm;
		} else if (shm_fd == -1)
			log_err("can neither create new shm nor access old");
	} else 
		log_err("couldn't create a new shm");

	/* only resize shared memory on first run */
	if (is_creator && (ftruncate(shm_fd, shm_size) == -1))
			log_err("ftruncate");

	struct shared_data *shm_data = mmap(NULL, shm_size, 
			              PROT_READ | PROT_WRITE,
			             MAP_SHARED, shm_fd, 0);
	if (shm_data == MAP_FAILED)
		log_err("mmap");

	/* on first run, initialize everything */
	if (is_creator) {
		/* initialize the mutex and conditional variable */
		pthread_mutexattr_t mattr;
		pthread_condattr_t cattr;
		pthread_mutexattr_init(&mattr);
		pthread_condattr_init(&cattr);

		/* we want them shared across processes, not private */
		pthread_mutexattr_setpshared(&mattr, 
				    PTHREAD_PROCESS_SHARED);
		pthread_condattr_setpshared(&cattr, 
				   PTHREAD_PROCESS_SHARED);

		/* point them at the shared memory struct */
		pthread_mutex_init(&shm_data->mutex, &mattr);
		pthread_cond_init(&shm_data->cond, &cattr);

		/* we've transferred the attributes to the struct
		 * and don't need these anymore */
		pthread_mutexattr_destroy(&mattr);
		pthread_condattr_destroy(&cattr);

		/* start at version 0 */
		shm_data->version = 0;

		/* zero out slots */
		memset(shm_data->slots, 0, sizeof(shm_data->slots));
	}

	/* main loop */
	const size_t sep_len = strlen(SEP);
	unsigned long local_version = 0; /* last processed version */
	char out_str[MAX_LEN];

	while (!terminate) {
		/* initialize string */
                size_t cur_len = 0; 
		out_str[0] = '\0';

		/* lock the mutex */
		pthread_mutex_lock(&shm_data->mutex);

		/* start the conditional wait within predicate loop:
		 * 	wait as long as not terminate +
		 * 	versions match
		 */
		while (!terminate && shm_data->version == local_version) {
			/* unlock mutex, put thread to sleep, and
			 * upon wake up lock (for this process) */
			pthread_cond_wait(&shm_data->cond, 
				   &shm_data->mutex);
		}

		/* thread wakes up! */
		/* check for signal first */
		if (terminate) {
			pthread_mutex_unlock(&shm_data->mutex);
			break;
		}

		/* gather all the slots' strings */
		char *dst = out_str;
                for (int i = 0; i < (int)NUM_MODULES; ++i) {
                        char *slot = shm_data->slots[i];
                        if (slot[0] == '\0')
                                continue;

			size_t slot_len = strlen(slot);
			/* add separator length if not first module */
			size_t need = slot_len + (cur_len ? sep_len : 0);

			/* overflow guard: log we've overflown,
			 * and only print the module strings that fit */
			if (cur_len + need >= MAX_LEN) {
				log_err("string too long\n");
				break;
			}

			/* first write separator to string if not first module */
			if (cur_len > 0) {
				/* the separator is defined in config.h */
				memcpy(dst, SEP, sep_len);
				dst += sep_len;
				cur_len += sep_len;
			}
			/* then write the full string to memory */
			memcpy(dst, slot, slot_len);
			dst += slot_len;
			cur_len += slot_len;
		}

		/* null terminate */
		*dst = '\0';
		/* update version */
		local_version = shm_data->version;
		/* we're done: unlock the mutex again */
		pthread_mutex_unlock(&shm_data->mutex);

		/* and finally output the string, if not empty
		 * we use write() to ensure we bypass any buffering */
		if (out_str[0] != '\0')
			write(1, out_str, cur_len);
		/* we assume this call works, and don't check for bytes written */
	}

	/* this part is only reached upon signal termination 
	 * update with signal name and clean up everything */
        printf("%s", strsignal(term_sig));

        if (munmap(shm_data, shm_size) == -1)
		log_err("munmap");
	if (shm_unlink(SHM_NAME) == -1)
		log_err("shm_unlink");
        close(shm_fd);

        return 0;
}

void
handle_signal(int sig)
{
	term_sig = sig;
	terminate = 1;
}
