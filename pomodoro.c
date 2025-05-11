#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "util.h"

typedef struct {
        int   cycles;      /* number of work periods             */
        float work_min;    /* minutes in a work period           */
        float short_min;   /* minutes in a short break           */
        float long_min;    /* minutes in a long break            */
        int   freq;        /* take long break every N cycles     */
} Pomodoro;

static const char *module_name = "pomodoro";

void countdown(float minutes, char mode);
void run_pomodoro(Pomodoro *p);

int
main(int argc, char *argv[])
{
#ifdef ___OPENBSD___
        if (pledge("stdio proc unix", "") == -1) {
                perror("pledge");
                return 1;
        }
#endif

        /* defaults */
        Pomodoro pom = {
            .cycles     = 4,
            .work_min   = 25.0,
            .short_min  = 5.0,
            .long_min   = 15.0,
            .freq = 4
        };

        /* no flags → run defaults */
        if (argc == 1) {
                run_pomodoro(&pom);
                return 0;
        }

        /* flags */
        if (argc > 2) {
                int ch;
                while ((ch = getopt(argc, argv, "n:t:s:l:f:")) != -1) {
                        switch (ch) {
                        case 'n': pom.cycles     = atoi(optarg);      break;
                        case 't': pom.work_min   = strtof(optarg,0);  break;
                        case 's': pom.short_min  = strtof(optarg,0);  break;
                        case 'l': pom.long_min   = strtof(optarg,0);  break;
                        case 'f': pom.freq       = atoi(optarg);      break;
                        default : fprintf(stderr, "bad args\n");      return 1;
                        }
                }
                if (optind != argc)     
                        return 1;

                run_pomodoro(&pom);
                return 0;
        }

        return 1;
}

void
countdown(float minutes, char mode)
{
        int sec = (int)lroundf(minutes * 60.0f);
        while (sec) {
                write_to_slot(module_name, "\r[%c] %02d:%02d", 
			      mode, sec / 60, sec % 60);
                sleep(1);
                --sec;
        }
}

void
run_pomodoro(Pomodoro *p)
{
	char mode;
        for (int i = 0; i < p->cycles; ++i) {
		mode = 'W';
                countdown(p->work_min, mode);

                if (i == p->cycles - 1)                
                        break;

                if ((i + 1) % p->freq == 0) {
			mode = 'L';
                        countdown(p->long_min, mode);
                } else {
			mode = 'S';
                        countdown(p->short_min, mode);
                }
	}
	write_to_slot(module_name, "\rdone");

	return;
}
