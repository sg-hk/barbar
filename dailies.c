#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"
#include "config.h"

static const char *mname = "dailies";

typedef struct {
	int year, month, day;
	int *dailies;
} shm_dailies_t;

int 
main(int argc, char *argv[]) 
{
#ifdef __OpenBSD__
	if (pledge("stdio", NULL) == -1) {
		log_err("%s: pledge failed", mname);
	}
#endif

	time_t now = time(NULL) - FIRSTHOUR * 3600;
	struct tm *tm_now = localtime(&now);
	int y = tm_now->tm_year + 1900;
	int m = tm_now->tm_mon + 1;
	int d = tm_now->tm_mday;

	if (TARGET_COUNT < 1) {
		w2s(mname, "no study targets");
		return 1;
	}

	/* Static memory for shared state (kept across invocations) */
	static shm_dailies_t shm_data = {0};

	/* Reset dailies if new day */
	if (shm_data.year != y || shm_data.month != m || shm_data.day != d) {
		shm_data.year = y;
		shm_data.month = m;
		shm_data.day = d;
		shm_data.dailies = calloc(TARGET_COUNT, sizeof(int));
	}

	char codes[TARGET_COUNT];
	int *time_adds = calloc(TARGET_COUNT, sizeof(int));

	/* Map domains to single-letter flags */
	for (int i = 0; i < TARGET_COUNT; ++i) {
		codes[i] = TARGETS[i].domain[0];
		for (int j = 0; j < i; ++j) {
			if (codes[i] == codes[j]) {
				log_err("%s: duplicate domain initial [%c] for [%s] and [%s]",
					mname, codes[i], TARGETS[i].domain, TARGETS[j].domain);
				w2s(mname, "duplicate initials");
				exit(1);
			}
		}
	}

	if (argc >= 2) {
		for (int i = 1; i < argc; i += 2) {
			if (i + 1 >= argc) {
				log_err("%s: missing time after %s", mname, argv[i]);
				w2s(mname, "missing time");
				return 1;
			}
			if (argv[i][0] != '-') {
				log_err( "%s: invalid flag %s", mname, argv[i]);
				w2s(mname, "invalid flag");
				return 1;
			}
			char flag = argv[i][1];
			int found = 0;
			for (int j = 0; j < TARGET_COUNT; ++j) {
				if (flag == codes[j]) {
					time_adds[j] = atoi(argv[i + 1]);
					found = 1;
					break;
				}
			}
			if (!found) {
				log_err( "%s: invalid flag %s", mname, argv[i]);
				w2s(mname, "invalid flag");
				return 1;
			}
		}

		/* Apply time additions to dailies */
		for (int i = 0; i < TARGET_COUNT; ++i) {
			int new_time = shm_data.dailies[i] + time_adds[i];
			shm_data.dailies[i] = (new_time < 999) ? new_time : 999;
		}
	}

	/* Prepare output message with remaining times */
	char out_msg[MSG_SIZE] = {0};
	int total_len = 0;

	for (int i = 0; i < TARGET_COUNT; ++i) {
		int time_left = TARGETS[i].time - shm_data.dailies[i];
		if (time_left < 0) time_left = 0;

		char buf[16];
		int lenbuf = snprintf(buf, sizeof(buf), "%c:%d ", codes[i], time_left);
		total_len += lenbuf;

		if (total_len < (int)sizeof(out_msg)) {
			strncat(out_msg, buf, sizeof(out_msg) - strlen(out_msg) - 1);
		}
	}

	/* Remove trailing space */
	int len = strlen(out_msg);
	if (len > 0 && out_msg[len - 1] == ' ') {
		out_msg[len - 1] = '\0';
	}

	w2s(mname, "%s", out_msg);
	free(time_adds);
	free(shm_data.dailies);
	return 0;
}
