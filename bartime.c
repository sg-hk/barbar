#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>

#include "util.h"

static const char *mname = "bartime";
static const char date_format[] = "%m月%d日（%A）%H:%M";
static volatile sig_atomic_t terminate = 0;
static volatile sig_atomic_t term_sig = 0;

static void handle_signal(int sig);

int
main(void)
{
	if (!setlocale(LC_TIME, "zh_CN.UTF-8"))
		w2s(mname, "setlocale failed");
	
	struct sigaction sa;
	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);  /* ctrl + c */
	sigaction(SIGTERM, &sa, NULL); /* ctrl + d */
	sigaction(SIGHUP, &sa, NULL);  /* window close */

	while (!terminate) {
		time_t now = time(NULL);
		struct tm *local_now = localtime(&now);
		char time_str[64];
		strftime(time_str, sizeof time_str, 
			 date_format, local_now);

		w2s(mname, time_str);

		/* sleep until the next minute */
		long secs_left = 60 - (now % 60);
		sleep(secs_left);
	}

	w2s(mname, "%s", strsignal(term_sig));

	return 0;
}

void
handle_signal(int sig) 
{
	term_sig = sig;
	terminate = 1;
}
