#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util.h"      /* w2s(), log_err(), handle_signal(), terminate */

static const char *mname = "music";
static volatile sig_atomic_t terminate = 0;
static int term_sig = 0;

static void  get_cmus(char *title, size_t, char *artist, size_t);
static void  get_volume(char *vol, size_t);
static void  handle_signal(int sig);

int
main(void)
{
        struct sigaction sa = {0};
        sa.sa_handler = handle_signal;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT,  &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        while (!terminate) {
                char title[128]  = "?";
                char artist[128] = "?";
                char vol[16]     = "?%";

                get_cmus(title, sizeof title, artist, sizeof artist);
                get_volume(vol, sizeof vol);

                char buf[300];
                snprintf(buf, sizeof buf, "%s - %s - %s%s", title, artist, vol, "%");
                w2s(mname, buf);

                /* sleep 2 seconds */
                struct timespec ts = { .tv_sec = 2, .tv_nsec = 0 };
                while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
                        continue;
        }

        w2s(mname, "%s", strsignal(term_sig));
        return 0;
}

/* helper definitions ----------------------------------------------------- */

/* Trim leading spaces */
static char *
ltrim(char *s)
{
        while (*s && isspace((unsigned char)*s)) ++s;
        return s;
}

/* extract title / artist from cmus-remote -Q output */
static void
get_cmus(char *title, size_t tlen, char *artist, size_t alen)
{
        FILE *fp = popen("cmus-remote -Q 2>/dev/null", "r");
        if (!fp)
                return;

        char line[256];
        while (fgets(line, sizeof line, fp)) {
                if (!strncmp(line, "tag title", 9)) {
                        char *p = ltrim(line + 9);
                        p[strcspn(p, "\r\n")] = '\0';
                        strncpy(title, p, tlen - 1);
                        title[tlen - 1] = '\0';
                } else if (!strncmp(line, "tag artist", 10)) {
                        char *p = ltrim(line + 10);
                        p[strcspn(p, "\r\n")] = '\0';
                        strncpy(artist, p, alen - 1);
                        artist[alen - 1] = '\0';
                }
        }
        pclose(fp);
}

/* extract “###%” from first line of pactl volume output */
static void
get_volume(char *vol, size_t vlen)
{
        FILE *fp = popen("pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null",
                         "r");
        if (!fp)
                return;

        char line[256];
        if (fgets(line, sizeof line, fp)) {
                char *slash = strchr(line, '/');           /* first “/” */
                if (slash) {
                        ++slash;
                        while (*slash && isspace((unsigned char)*slash))
                                ++slash;
                        char *end = strchr(slash, '%');
                        if (end && end > slash && (size_t)(end - slash + 1) < vlen) {
                                memcpy(vol, slash, end - slash + 1);
                                vol[end - slash + 1] = '\0';
                        }
                }
        }
        pclose(fp);
}

static void  
handle_signal(int sig)
{
	term_sig = sig;
	terminate = 1;
}
