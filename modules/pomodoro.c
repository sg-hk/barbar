#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "../config.h"
#include "../util.h"

#define MODULE_NAME "pomodoro"

typedef struct Pomodoro {
        int n;
        float wtime;
        float stime;
        float ltime;
        int freq;
} Pomodoro;

int is_number(const char*);
void countdown(float minutes);
void do_pomodoro(Pomodoro *pom);
int solve_pomodoro(int total_time, Pomodoro *pom);

int 
main(int argc, char *argv[])
{

        /* set default pomodoro values */
        Pomodoro pom;
        pom.n = 4;
        pom.wtime = 25;
        pom.stime = 5;
        pom.ltime = 15;
        pom.freq = 4;

        /* no args: default pomodoro */
        if (argc == 1) {
                do_pomodoro(&pom);
                exit(0);
        }

        /* single numerical argument = total time, derive other arguments */
        if (argc == 2 && is_number(argv[1])) {
                int total_time = atoi(argv[1]);
                if (!solve_pomodoro(total_time, &pom))
                        log_err("main: couldn't solve pomodoro");
                do_pomodoro(&pom);
                exit(0);
        } 

        /* more than one arg: custom pomodoro session */
        if (argc > 2) {
                int ch;
                while ((ch = getopt(argc, argv, "n:t:s:l:")) != -1) {
                        switch (ch) {
                                case 'n':
                                        pom.n = atoi(optarg);
                                        break;
                                case 't':
                                        pom.wtime = atoi(optarg);
                                        break;
                                case 's':
                                        pom.stime = atoi(optarg);
                                        break;
                                case 'l':
                                        pom.ltime = atoi(optarg);
                                        break;
                                default:
                                        log_err("main: invalid arguments");
                                        exit(1);
                        }
                }
                argc -= optind;
                argv += optind;

                if (argc != 0)
                        log_err("pomodoro: argc");

                do_pomodoro(&pom);
                exit(0);
        }

        /* exit on invalid arguments */
        log_err("main: invalid arguments");
        exit(1);
}

int 
is_number(const char *s) 
{
        if (!*s) return 0;
        for (; *s; ++s) {
                if ((*s < '0' || *s > '9') && !(*s == '.')) return 0;
        }
        return 1;
}

void 
countdown(float minutes)
{
        int seconds = minutes * 60;
        while (seconds) {
                write_to_slot(MODULE_NAME, "%0.2d:%0.2d", 
                                seconds / 60, seconds % 60);
                sleep(1);
                --seconds;
        }
        return;
}

void 
do_pomodoro(Pomodoro *pom)
{
        for (int i = 0; i < pom->n; ++i) {
                countdown(pom->wtime);
                if ((i + 1) % pom->freq == 0)
                        countdown(pom->ltime);
                else
                        countdown(pom->stime);
        }
        return;
}

int 
solve_pomodoro(int T, Pomodoro *pom)
{
        /* derive other arguments with maximum 20 sessions */
        for (int n = 1; n <= 20; n++) {
                double factor = n + 0.25 * (n - 1);  // factor = N + 0.25*(N-1)
                double work = T / factor;
                if (work >= 5.0) {  // minimum work duration.
                        pom->n = n;
                        pom->wtime = work;
                        pom->stime = work / 5.0;
                        pom->ltime = 2 * pom->stime;
                        return 1;  // success
                }
        }
        return 0;  // no valid solution found
}
