#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "../util.h"
#include "../config.h" /* for MAX_READ */
#include "config_dailies.h"

static const char *mname = "dailies";

int 
main(int argc, char *argv[])
{
	/* preliminary checks */
	if (TARGET_COUNT < 1) {
		w2s(mname, "no study targets");
		return 1;
	}

	int max_len = MAX_READ;
	int date_len = 10; /* YYYY/MM/DD */
	int times_len = (TARGET_COUNT) * (1 + 2 + 3); /* |a:000|b:000|... */
	int str_len = date_len + times_len;
	if (str_len > max_len) {
		w2s(mname, "too many study targets");
		return 1;
	}

	/* get valid codes */
	char codes[TARGET_COUNT];
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

	/* parse opt args */
	char optstr[TARGET_COUNT * 2 + 1];
	int pos = 0;
	for (int i = 0; i < TARGET_COUNT; ++i) {
		optstr[pos++] = codes[i];
		optstr[pos++] = ':'; 
	}
	optstr[pos] = '\0';

	int ch;
	int *time_adds = calloc(TARGET_COUNT, sizeof(int));
	if (!time_adds) { w2s(mname, "calloc time_adds"); exit(1); }

	while ((ch = getopt(argc, argv, optstr)) != -1) {
		int valid = 0;
		for (int i = 0; i < TARGET_COUNT; ++i) {
			if (ch == codes[i]) {
				char *endptr;
				int val = strtol(optarg, &endptr, 10);
				if (*endptr != '\0' || val < 0) {
					log_err("%s: invalid value for -%c: %s", mname, ch, optarg);
					w2s(mname, "invalid arg value");
					exit(1);
				}
				time_adds[i] += val;
				valid = 1;
				break;
			}
		}
		if (!valid) {
			w2s(mname, "invalid arg");
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	/* open or create log for read/write */
	char *home = getenv("HOME");
	if (!home) {
		w2s(mname, "home envp not set");
		return 1;
	}
	char path[128];
	snprintf(path, sizeof path, "%s%s", home, DAILIES_LOG_PATH);
	int fd = open(path, O_RDWR | O_CREAT, 0644);
	if (fd < 0) {
		w2s(mname, "error opening log");
		return 1;
	}

	/* get today */
	time_t now = time(NULL) - FIRSTHOUR * 3600;
	struct tm *tm_now = localtime(&now);
	int y = tm_now->tm_year + 1900;
	int m = tm_now->tm_mon + 1;
	int d = tm_now->tm_mday;

	char log_str[str_len + 1];
	ssize_t nread = read(fd, log_str, str_len);
	if (nread != str_len) {
		if (nread < 0) {  /* read error */
			w2s(mname, "error reading log");
			close(fd);
			return 1;
		}
		/* new or incomplete log, initialize it */
		lseek(fd, 0, SEEK_SET);
		ftruncate(fd, 0);

		snprintf(log_str, sizeof log_str, "%04d/%02d/%02d", y, m, d);
		for (int i = 0; i < TARGET_COUNT; ++i)
			strlcat(log_str, "|000", sizeof log_str);

		if (write(fd, log_str, str_len) != str_len) {
			w2s(mname, "error initializing log");
			close(fd);
			return 1;
		}

		/* move back to start for parsing the newly written log */
		lseek(fd, 0, SEEK_SET);
		if (read(fd, log_str, str_len) != str_len) {
			w2s(mname, "error reading initialized log");
			close(fd);
			return 1;
		}
	}
	log_str[str_len] = '\0';

	/* parse log date */
	int yy, mm, dd;
	char *tok = strtok(log_str, "/");
	if (!tok) { w2s(mname, "error parsing year"); exit(1); }
	yy = atoi(tok);
	tok = strtok(NULL, "/");
	if (!tok) { w2s(mname, "error parsing month"); exit(1); }
	mm = atoi(tok);
	tok = strtok(NULL, "|");
	if (!tok) { w2s(mname, "error parsing day"); exit(1); }
	dd = atoi(tok);

	/* reset if new day */
	if (yy != y || mm != m || dd != d) {
		lseek(fd, 0, SEEK_SET);  
		ftruncate(fd, 0);

		snprintf(log_str, sizeof log_str, "%04d/%02d/%02d", y, m, d);
		for (int i = 0; i < TARGET_COUNT; ++i)
			strlcat(log_str, "|000", sizeof log_str);

		if (write(fd, log_str, str_len) != str_len) {
			w2s(mname, "error resetting log");
			close(fd);
			return 1;
		}

		/* reset parsing */
		strcpy(log_str, log_str); 
		tok = strtok(log_str, "/");  /* advance strtok state */
		tok = strtok(NULL, "/");
		tok = strtok(NULL, "|");
	}

	/* parse log times */
	int *times_init = malloc(TARGET_COUNT * sizeof(int));
	if (!times_init) { w2s(mname, "error malloc times_init"); exit(1); }
	for (int i = 0; i < TARGET_COUNT; ++i) {
		tok = strtok(NULL, "|");
		if (!tok) { w2s(mname, "error parsing times_init"); exit(1); }
		times_init[i] = atoi(tok);
	}

	close(fd);

	/* update total times and write back log */
	int *total_time = malloc(TARGET_COUNT * sizeof(int));
	if (!total_time) { w2s(mname, "error malloc total_time"); exit(1); }

	for (int i = 0; i < TARGET_COUNT; ++i) {
		total_time[i] = times_init[i] + time_adds[i];
		if (total_time[i] > 999)  // Clamp to 999 to avoid overflow in log
			total_time[i] = 999;
	}

	/* rebuild log string */
	char new_log[str_len + 1];
	snprintf(new_log, sizeof new_log, "%04d/%02d/%02d", y, m, d);

	for (int i = 0; i < TARGET_COUNT; ++i) {
		char buf[8];
		snprintf(buf, sizeof buf, "|%03d", total_time[i]);
		strlcat(new_log, buf, sizeof new_log);
	}

	/* reopen and write updated log */
	fd = open(path, O_WRONLY | O_TRUNC);
	if (fd < 0) {
		w2s(mname, "error reopening log for writing");
		exit(1);
	}
	if (write(fd, new_log, str_len) != str_len) {
		w2s(mname, "error writing updated log");
		close(fd);
		exit(1);
	}
	close(fd);

	/* prepare output message with remaining times */
	char out_msg[MSG_SIZE] = {0};
	int total_len = 0;

	for (int i = 0; i < TARGET_COUNT; ++i) {
		int time_left = TARGETS[i].time - total_time[i];
		if (time_left < 0) time_left = 0;

		char buf[16];
		int lenbuf = snprintf(buf, sizeof buf, "%c:%d ", codes[i], time_left);
		total_len += lenbuf;

		if (total_len < (int)sizeof(out_msg)) {
			strncat(out_msg, buf, sizeof(out_msg) - strlen(out_msg) - 1);
		}
	}

	/* remove trailing space */
	int len = strlen(out_msg);
	if (len > 0 && out_msg[len - 1] == ' ') {
		out_msg[len - 1] = '\0';
	}

	w2s(mname, "%s", out_msg);

	/* cleanup */
	free(time_adds);
	free(times_init);
	free(total_time);
	return 0;
}
