/* TODO change errors to log_err and very succinct w2s */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <time.h>

#include "util.h"
#include "config.h"

static const char *mname = "dailies";

int
main(int argc, char *argv[])
{
#ifdef __OPENBSD__
	/* TODO correct pledge */
	if (pledge("stdio rpath wpath unveil", NULL) == -1) {
		w2s(mname, "pledge");
		exit(1);
	}
#endif

	time_t now = time(NULL) - FIRSTHOUR * 3600;
	struct tm *tm = localtime(&now);
	int y = tm->tm_year + 1900;
	int m = tm->tm_mon + 1;
	int d = tm->tm_mday;

	if (TARGET_COUNT < 1) {
		w2s(mname, "no study targets");
		return 1;
	}

	char path[128];
	snprintf(path, sizeof(path), "%s%s", getenv("HOME"), LOGPATH);

#ifdef __OPENBSD__
	/* TODO add shm to unveil */
	if (unveil(path, "rw") == -1) {
		w2s(mname, "unveil");
		exit(1);
	}
	if (unveil(NULL, NULL) == -1) { // lock unveil
		w2s(mname, "unveil lock");
		exit(1);
	}
	if (pledge("stdio rpath wpath", NULL) == -1) {
		w2s(mname, "pledge");
		exit(1);
	}
#endif

	FILE *log = fopen(path, "r+");
	if (!log) { 
		log = fopen(path, "w+");  
		if (!log) {
			w2s(mname, "failed to create dailies log");
			exit(1);
		}
		char ndate[11];
		snprintf(ndate, sizeof(ndate), "%04d/%02d/%02d", y, m, d);
		int init_len = (TARGET_COUNT * 4) + 10 + 1; 
		char *init_line = calloc(init_len, 1);

		for (int i = 0; i < TARGET_COUNT; ++i)
			strncat(init_line, "000|", 4);
		strncat(init_line, ndate, 10);

		fputs(init_line, log);
		fflush(log);

		free(init_line);
	}

	/* 		   "000" + '|'     date */
	int max_line_len = (3 + 1) * TARGET_COUNT + 10 + 1;
	char line[max_line_len];

	/* get just the last line */
	while (fgets(line, sizeof(line), log))
		;
	int line_len = strlen(line);
	if (line_len == 0) 
		line[0] = '\0'; 
	else 
		line[line_len - 1] = '\0'; /* remove \n if log not empty */ 
	
	int line_start_pos = ftell(log) - line_len;

	/* parse date field */
	char *date_field = strrchr(line, '|');
	if (!date_field) { w2s(mname, "parsing date field"); exit(1); }
	date_field += 1;

	char *tok = strtok(date_field, "/");
	if (!tok) { w2s(mname, "strtok date year"); exit(1); }
	int yy = atoi(tok);
	tok = strtok(NULL, "/");
	if (!tok) { w2s(mname, "strtok date month"); exit(1); }
	int mm = atoi(tok);
	tok = strtok(NULL, "|");
	if (!tok) { w2s(mname, "strtok date day"); exit(1); }
	int dd = atoi(tok);

	/* prepare the variables to update today's record */
	int dailies[TARGET_COUNT];
	memset(dailies, 0, sizeof(dailies));
	char codes[TARGET_COUNT];
	int time_adds[TARGET_COUNT];
	memset(time_adds, 0, sizeof(time_adds));
	char ndate[11];
	snprintf(ndate, sizeof(ndate), "%04d/%02d/%02d", y, m, d);
	char nline[max_line_len];
	nline[0] = '\0';

	/* create the record if it doesn't exist */
	if (yy != y || mm != m || dd != d) {
		for (int i = 0; i < TARGET_COUNT; ++i)
			strncat(nline, "000|", 4);
		strncat(nline, ndate, 10);
		fseek(log, 0, SEEK_END);
		fputs(nline, log);
		fflush(log);

		line_start_pos = ftell(log) - strlen(nline);
		goto skip_parse;
	}

	/* parse the dailies */
	for (int i = 0; i < TARGET_COUNT; ++i) {
		tok = (i == 0) ? 
		      strtok(line, "|") : strtok(NULL, "|");
		if (!tok) { 
			w2s(mname, "dailies parsing"); 
			exit(1); 
		}
		dailies[i] = atoi(tok);
	}

skip_parse:
	/* no study times to add: only print time left */
	if (argc < 2)
		goto skip_update;

	/* otherwise prepare the codes and compare with argv flags 
	 * the idea is that each domain's first letter is a flag:
	 * for the domain "theory" -t 25 will add 25 minutes to the log */
	for (int i = 0; i < TARGET_COUNT; ++i) {
		codes[i] = TARGETS[i].domain[0];
		for (int j = 0; j < i; ++j) {
			if (codes[i] == codes[j]) {
				fprintf(stderr, 
					"can't have same first letter [%c] "
					"for domains [%s] and [%s]\n",
					codes[i],
					TARGETS[i].domain,
					TARGETS[j].domain);
				return 1;
			}
		}
	}

	for (int i = 1; i < argc; i += 2) {
		if (i + 1 >= argc) {
			w2s(mname, 
				"missing time value after %s", argv[i]);
			return 1;
		}
		if (argv[i][0] != '-') {
			w2s(mname, 
				"expected flag starting with -, got: %s", 
				argv[i]);
			return 1;
		}
		char flag = argv[i][1];

		int found = 0;
		for (int j = 0; j < TARGET_COUNT; ++j) {
			if (flag == codes[j]) {
				time_adds[j] = atoi(argv[i+1]);
				found = 1;
				break;
			}
		}
		if (!found) {
			w2s(mname, "unknown domain flag: -%c\n", flag);
			return 1;
		}
	}

	/* edit record in place */
	for (int i = 0; i < TARGET_COUNT; ++i) {
		int total_time = (dailies[i] + time_adds[i] < 999) ?
			         dailies[i] + time_adds[i] : 999;
		dailies[i] = total_time;
		char ndaily[5];
		snprintf(ndaily, sizeof(ndaily), "%03d|", dailies[i]);
		strncat(nline, ndaily, 4);
	}
	strncat(nline, ndate, 10);
	fseek(log, line_start_pos, SEEK_SET);
	fputs(nline, log);

skip_update:
	fclose(log);

#ifdef __OPENBSD__
	if (pledge("stdio", NULL) == -1) {
		w2s(mname, "pledge");
		exit(1);
	}
#endif

	int times[TARGET_COUNT];
	char update_msg[128] = {0};
	char buf[6];
	int total_len = 0;

	for (int i = 0; i < TARGET_COUNT; ++i) {
		int timeleft = TARGETS[i].time - dailies[i];
		times[i] = (timeleft > 0) ? timeleft : 0;
		snprintf(buf, sizeof buf, "%c:%d ", codes[i], times[i]);
		int lenbuf = strlen(buf);
		total_len += lenbuf;
		strncat(update_msg, buf, sizeof update_msg - total_len - 1);
	}

	if (total_len > 1)
		update_msg[total_len-2] = '\0'; /* remove last space */

	w2s(mname, "%s", update_msg);

	return 0;
}
