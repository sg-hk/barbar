#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "../util.h"
#include "config_bartime.h"

static const char *mname = "bartime";
static volatile sig_atomic_t terminate = 0;

void handle_signal(int sig);

int
main(void)
{
#ifdef __OpenBSD__
	/* TODO test the validity of this */
	if (pledge("stdio rpath wpath unveil", NULL) == -1) {
		w2s(mname, "pledge");
		return 1;
	}
	if (unveil(SHM_NAME, "rw") == -1) {
		w2s(mname, "unveil");
		return 1;
	}
	if (unveil(NULL, NULL) == -1) {
		w2s(mname, "unveil lock");
		return 1;
	}
	if (pledge("stdio", NULL) == -1) {
		w2s(mname, "pledge (final)");
		return 1;
	}
#endif

	struct sigaction sa;
	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	while (!terminate) {
		time_t now = time(NULL);
		struct tm *local_now = localtime(&now);
		char time_str[32];
		strftime(time_str, sizeof time_str, 
			 date_format, local_now);

		w2s(mname, time_str);

		/* sleep until the next minute */
		long secs_left = 60 - (now % 60);
		sleep(secs_left);
	}

	w2s(mname, "user terminated");

	return 0;
}

void handle_signal(int sig) {
    (void)sig;  
    terminate = 1;
}
