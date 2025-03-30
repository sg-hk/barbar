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

int is_number(const char*);
void countdown(int);
void do_pomodoro(int n, int wtime, int stime, int ltime, int freq);

int
main(int argc, char *argv[])
{
        int ch;
        int n = 4;
        int wtime = 25;
        int stime = 5;
        int ltime = 15;
        int freq = 3;

        /* 
         * no args: default pomodoro, 4 cycles of 25 minutes 
         * with short breaks of 5m and long breaks of 15m
         * alternating every 3 work cycles
         */
        if (argc == 1)
                do_pomodoro(n, wtime, stime, ltime, freq);

        /* single numerical argument: simple timer */
        if (argc == 2 && is_number(argv[1]))
                countdown(atoi(argv[1]));

        /* more than one arg: custom pomodoro session */
        if (argc > 2) {
                while ((ch = getopt(argc, argv, "n:t:s:l:f:")) != -1) {
                        switch (ch) {
                                case 'n':
                                        n = atoi(optarg);
                                        break;
                                case 't':
                                        wtime = atoi(optarg);
                                        break;
                                case 's':
                                        stime = atoi(optarg);
                                        break;
                                case 'l':
                                        ltime = atoi(optarg);
                                        break;
                                case 'f':
                                        freq = atoi(optarg);
                                        break;
                                default:
                                        exit(1);
                        }
                }
                argc -= optind;
                argv += optind;

                if (argc != 0)
                        log_err("pomodoro: argc");

                do_pomodoro(n, wtime, stime, ltime, freq);
        }

        exit(0);
}

int
is_number(const char *s) 
{
    if (!*s) return 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9') return 0;
    }
    return 1;
}

void
countdown(int minutes)
{
        int seconds = minutes * 60;
        while (seconds) {
                write_to_slot(MODULE_NAME, "%0.2d:%0.2d", seconds / 60, seconds % 60);
                sleep(1);
                --seconds;
        }
        return;
}

void
do_pomodoro(int n, int wtime, int stime, int ltime, int freq)
{
        for (int i = 0; i < n; ++i) {
                countdown(wtime);
                if ((i + 1) % freq == 0)
                        countdown(ltime);
                else
                        countdown(stime);
        }
        return;
}
