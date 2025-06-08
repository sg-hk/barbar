#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

/* module name for barbar */
static const char mname[]            = "cpom";

/* file paths */
static const char CTIMER[]           = "/.local/share/cpom";
static const char CCQ_ZH[]           = "/.local/share/ccq/zh"; 

/* notification titles */
static const char work_title[]       = "工作";
static const char break_title[]      = "休息";
static const char long_break_title[] = "长休";
static const char over_title[]        = "完成";

/* signal handling */
static volatile sig_atomic_t terminate = 0;
static int                   term_sig  = 0;

typedef struct PomState {
        int  file_flag;
        char start_msg[128];
        char over_msg[64];
        char startfp[64];
        char endfp[64];
        char overfp[64];
} PomState;

static void   countdown_timer(int seconds, PomState *state);
static void   initialize_strings(PomState *state, int n, float ptime, float sbktime, float lbktime, int total_time, int work_time);
static void   notify(const char *title, const char *msg);
static void   play_sound(const char *filepath);
static void   pomodoro(int n, float ptime, float sbktime, float lbktime, int frq, PomState *state);
static void   printhelp(const char *progname);
static char **get_words(int *total);
static void   handle_signal(int sig);

int 
main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	/* defaults for pomodoro */
        int n = 5;
        float ptime = 25;
        float sbktime = 5;
        float lbktime = 15;
        int frq = 4;
        PomState state;
        state.file_flag = 0;
        int opt;
	char *endptr;

	struct sigaction sa = {0};
	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT,  &sa, NULL);

        while ((opt = getopt(argc, argv, "n:t:s:l:f:Fh")) != -1) {
                switch (opt) {
		case 'n':
			n = atoi(optarg);
			if (n <= 0) {
				fprintf(stderr,
						"invalid number of pomodoros\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 't':
			errno = 0;
			ptime = strtof(optarg, &endptr);
			if (errno != 0 || endptr == optarg || *endptr != '\0' || ptime <= 0) {
				fprintf(stderr, "invalid pomodoro time\n");
				exit(EXIT_FAILURE);
			}
			break;

		case 's':
			errno = 0;
			sbktime = strtof(optarg, &endptr);
			if (errno != 0 || endptr == optarg || *endptr != '\0' || sbktime <= 0) {
				fprintf(stderr, "invalid short break time\n");
				exit(EXIT_FAILURE);
			}
			break;

		case 'l':
			errno = 0;
			lbktime = strtof(optarg, &endptr);
			if (errno != 0 || endptr == optarg || *endptr != '\0' || lbktime <= 0) {
				fprintf(stderr, "invalid long break time\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'f':
			frq = atoi(optarg);
			if (frq <= 0) {
				fprintf(stderr, "invalid frequency\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'F':
			state.file_flag = 1;
			break;
		case 'h':
		default:
			printhelp(argv[0]);
                }
        }

        int work_time    = n * ptime;
        int breaks       = n - 1;
        int long_breaks  = (frq > 0) ? breaks / frq : 0;
        int short_breaks = breaks - long_breaks;
        int total_time   = work_time + short_breaks * sbktime +
                           long_breaks * lbktime;

        initialize_strings(&state, n, ptime, sbktime, lbktime,
                           total_time, work_time);
        pomodoro(n, ptime, sbktime, lbktime, frq, &state);
        return 0;
}

static void 
notify(const char *title, const char *msg)
{
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "notify-send '%s' '%s'", title, msg);
        system(cmd);
}

static void
play_sound(const char *file)
{
    pid_t pid = fork();
    if (pid == -1) { 
	    perror("fork"); 
	    return; 
    }

    if (pid == 0) {  
        if (fork() == 0) {     
            int devnull = open("/dev/null", O_RDWR);
            if (devnull != -1) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                if (devnull > 2) close(devnull);
            }
            execlp("mpv", "mpv", "--no-terminal", file, (char *)0);
            _exit(1); 
        }
        _exit(0);    
    }

    /* parent reaps first child, grand-child is reparented to init → no zombie */
    while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
        ;
}

/* initialize runtime strings in state */
static void 
initialize_strings(PomState *state, 
                   int n, float ptime, float sbktime, float lbktime, 
                   int total_time, int work_time)
{

        char *home = getenv("HOME");
        if (!home) {
                fprintf(stderr, "error: home not set\n");
                exit(EXIT_FAILURE);
        }

        snprintf(state->startfp, sizeof(state->startfp), "%s%s%s",
                        home, CTIMER, "start.mp3");
        snprintf(state->endfp, sizeof(state->endfp), "%s%s%s",
                        home, CTIMER, "end.mp3");
        snprintf(state->overfp, sizeof(state->overfp), "%s%s%s",
                        home, CTIMER, "over.mp3");

        snprintf(state->start_msg, sizeof(state->start_msg),
			"总计 %d 分钟；工作 %d 分钟\n"
			"次数=%d 工作=%.1f 休息=%.1f 长休=%.1f",
			total_time, work_time,
                        n, ptime, sbktime, lbktime);
        snprintf(state->over_msg, sizeof(state->over_msg),
			"完成了 %d 个番茄钟", n);
}

static void
countdown_timer(int seconds, PomState *state)
{
        char line[12];

        for (int elapsed = 0; elapsed < seconds; ++elapsed) {
                if (terminate)
			break;

                int left = seconds - elapsed;
                snprintf(line, sizeof line, "%02u:%02u", left / 60, left % 60);

                if (state->file_flag)
                        w2s(mname, line);
                else {
                        printf("\r%s", line);
                        fflush(stdout);
                }
                sleep(1);
        }
	/* TODO: remove the below and handle blocking signal handling correctly */
	/* so that the loop check works */
	if (terminate) {
		if (state->file_flag)
			w2s(mname, "%s", strsignal(term_sig));
		else
			puts("\nSIGINT");
		exit(1);
	}

        if (state->file_flag)
                w2s(mname, "完成");
        else
                puts("\r完成了");
}

/* execute pomodoro cycles */
static void 
pomodoro(int n, float ptime, float sbktime, float lbktime, int frq, 
         PomState *state)
{
	int total = 0;
	char **words = get_words(&total);
	srand(time(NULL)); 

        for (int i = 0; i < n; ++i) {
                play_sound(state->startfp);
                char pom_msg[128];
                snprintf(pom_msg, sizeof(pom_msg),
                                "[%d/%d] 番茄钟: %.1f 分钟", i + 1, n, ptime);
		notify(work_title, pom_msg);
                countdown_timer(ptime * 60, state);
                if (i == n - 1) {
                        play_sound(state->overfp);
                        notify(over_title, state->over_msg);
                        return;
                }
                play_sound(state->endfp);
                
		char bk_msg[128];
		char *word = words[rand() % total];
		if ((i + 1) % frq == 0) {
			snprintf(bk_msg, sizeof(bk_msg),
					"[%d/%d] 长休: %.1f m\n"
					"%s",
					i + 1, n - 1, lbktime, word);
			notify(long_break_title, bk_msg);
			countdown_timer(lbktime * 60, state);
		} else {
			snprintf(bk_msg, sizeof(bk_msg),
					"[%d/%d] 休息: %.1f m\n"
					"%s",
					i + 1, n - 1, sbktime, word);
			notify(break_title, bk_msg);
			countdown_timer(sbktime * 60, state);
		}

        }
	free(words);
}



static void 
printhelp(const char *progname)
{
        fprintf(stderr,
                        "usage: %s\n"
                        "   [-n number of pomodoros]\n"
                        "   [-t pomodoro time in minutes]\n"
                        "   [-s short break time in minutes]\n"
                        "   [-l long break time in minutes]\n"
                        "   [-f frequency for long break]\n"
                        "   [-F output countdown to pipe]\n"
                        "   [-h print this help]\n"
                        "cpom v1.0\n",
                        progname);
        exit(EXIT_FAILURE);
}

static char **get_words(int *total)
{
	if (total) *total = 0;

	char path[128];
	snprintf(path, sizeof path, "%s%s", getenv("HOME"), CCQ_ZH);

	FILE *fp = fopen(path, "r");
	if (!fp) return NULL;

	char **words = NULL;
	size_t cap = 0, n = 0;

	char *line = NULL;
	size_t len = 0;
	while (getline(&line, &len, fp) != -1) {

		/* trim trailing newline / carriage return */
		char *eol = strpbrk(line, "\r\n");
		if (eol) *eol = '\0';

		/* want the 3rd ‘|’-separated field */
		char *tok = strtok(line, "|"); 
		tok = strtok(NULL, "|");      
		tok = strtok(NULL, "|");     
		if (!tok) continue;

		if (n == cap) {                       
			cap = cap ? cap * 2 : 32;
			char **tmp = realloc(words, (cap + 1) * sizeof *tmp);
			if (!tmp) { 
				perror("realloc"); 
				break; 
			}
			words = tmp;
		}
		words[n++] = strdup(tok);        
	}
	free(line);
	fclose(fp);

	if (words) words[n] = NULL;            
	if (total) *total = n;
	return words;
}

static void  
handle_signal(int sig)
{
	term_sig = sig;
	terminate = 1;
}
