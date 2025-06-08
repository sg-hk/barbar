/* 
 * ccqwatch updates barbar with the total count of due cards

 * the due count can change in three ways:
 *  - time passing
 *  - the study list changing (adds, reviews)

 * hence ccq first parses the study list, updating the due count

 * it then idles, waiting either for:
 *  - a time differential to elapse (i.e., cards becoming due over time), or
 *  - a file change event, which triggers a rebuild of the epochs array
 
 * this approach ensures optimal sleep cycles (called 'naps')
 * and minimizes monitoring overhead
 
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

static const char            mname[]   = "ccqwatch";
static const char            suffix[]  = "个词待复习";
static const char            done[]    = "每个词都复习了";
static volatile sig_atomic_t terminate = 0;
static int                   term_sig  = 0;

typedef struct {
	time_t *naps;
	int     cnt;
	int     cnt_ft;
} NapStack;

static int      cmp_time(const void *a, const void *b);
static void     die(const char *fmt, ...);
static void     handle_signal(int sig);
static NapStack parse_due_times(const char *path);

int
main(void)
{
	char     path[PATH_MAX], buf[4096];
	char    *home;
	int      in_fd, wd, i;
	struct   sigaction sa;
	struct   timespec timeout;
	sigset_t sigmask_all, sigmask_none;
	struct   pollfd pfd;
	NapStack ns;

	char       rdbuf[11] = {0};
	const char sl[]      = "/.local/share/ccq/zh"; 

	/* initialize signal handling */
	sigemptyset(&sigmask_all);
	sigaddset(&sigmask_all, SIGINT);
	sigaddset(&sigmask_all, SIGTERM);
	sigprocmask(SIG_BLOCK, &sigmask_all, NULL); 

	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	sigemptyset(&sigmask_none);

	/* get path */
	home = getenv("HOME");
	if (!home)
		die("home envp");
	snprintf(path, sizeof(path), "%s%s", home, sl);

	/* initialize inotify */
	in_fd = inotify_init1(IN_NONBLOCK);
	if (in_fd < 0)
		die(mname, "inotify_init");
	wd = inotify_add_watch(in_fd, path, IN_MODIFY | IN_CLOSE_WRITE);
	if (wd < 0)
		die(mname, "watch failed");

	/* parse file */
restart:
	ns = parse_due_times(path);
	if (!ns.naps)
		die(mname, "parse fail");

	/* print current dues */
	if (ns.cnt < 1)
		w2s(mname, done);
	else
		w2s(mname, "%d%s", ns.cnt, suffix);

	/* watch file */
	pfd.fd = in_fd;
	pfd.events = POLLIN;

	for (i = 0; i < ns.cnt_ft; ++i) {
		if (terminate) {
			w2s(mname, "%s", strsignal(term_sig));
			free(ns.naps);
			break;
		}

		/* sleep between dues, wake up upon file change or to update bar */
		timeout.tv_sec = ns.naps[i];
		timeout.tv_nsec = 0;

		switch (ppoll(&pfd, 1, &timeout, &sigmask_none)) {
		case -1:
			/* error: retry on EINTR */
			if (errno == EINTR)
				continue;
			die("ppoll: %s", strerror(errno));
		case 0:
			/* one nap has elapsed: update bar with new due count */
			++ns.cnt;
			w2s(mname, "%d%s", ns.cnt, suffix);
			break;
		default:
			/* file has changed: drain inotify & parse file */
			read(in_fd, buf, sizeof buf); 
			free(ns.naps);
			goto restart;
		}
	}

	inotify_rm_watch(in_fd, wd);
	close(in_fd);

	return 0;
}

static int 
cmp_time(const void *a, const void *b)
{
    time_t t1 = *(const time_t *)a;
    time_t t2 = *(const time_t *)b;
    return (t1 > t2) - (t1 < t2);
}

void
handle_signal(int sig)
{
	terminate = 1;
	term_sig  = sig;
}

void
die(const char *fmt, ...)
{
	char msg[64];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	w2s(mname, msg);
	log_err(msg);
	exit(1);
}

NapStack
parse_due_times(const char *path)
{
	char    *addr, *cur;
	char     rdbuf[11];
	int      i, fd, cap, cnt, cnt_ft;
	size_t   length;
	struct   stat sb;
	time_t  *naps, *notdues;
	time_t   gap, now;
	NapStack result;

	result.naps   = NULL;
	result.cnt    = 0;
	result.cnt_ft = 0;

	/* open study list, mmap it */
	fd = open(path, O_RDONLY);
	if (fd < 0 || fstat(fd, &sb) < 0)
		die("open/fstat");

	length = sb.st_size;
	addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
		die("mmap");

	/* gather dues and notdues */
	cap = 512;
	notdues = malloc(cap * sizeof(time_t));
	if (!notdues)
		die("malloc notdues");

	now = time(NULL);
	cnt = cnt_ft = 0;
	cur = addr;
	while (cur + 10 < addr + length) {
		char   *nl;
		time_t  epoch;
		time_t *tmp;

		if (terminate)
			die("%s", strsignal(term_sig));

		/* get epoch */
		memcpy(rdbuf, cur, 10);
		rdbuf[10] = '\0';
		epoch = (time_t)strtol(rdbuf, NULL, 10);

		/* bound check */
		if (cnt_ft >= cap) {
			cap *= 2;
			tmp  = realloc(notdues, cap * sizeof(time_t));
			if (!tmp)
				die("realloc notdues");
			notdues = tmp;
		}

		/* process as due / notdue */
		if (epoch <= now)
			++cnt;
		else 
			notdues[cnt_ft++] = epoch;

		/* find new line of break on EOF */
		nl = memchr(cur, '\n', addr + length - cur);
		if (!nl) 
			break;

		/* point to start of new line */
		cur = nl + 1;
	}

	munmap(addr, length);
	close(fd);

	qsort(notdues, cnt_ft, sizeof(time_t), cmp_time);

	naps = malloc(cnt_ft * sizeof(time_t));
	if (!naps)
		die("naps malloc");

	gap = now;
	for (int i = 0; i < cnt_ft; ++i) {
		naps[i] = notdues[i] - gap;
		gap += naps[i];
	}
	free(notdues);

	result.naps = naps;
	result.cnt = cnt;
	result.cnt_ft = cnt_ft;
	return result;
}
