#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#include "../util.h"

static const char *mname = "ccq";
static const char *ccq_suffix = "/.local/share/ccq/zh";

int
main(void)
{
	const int epoch_len = 10;

	char ch;
	char *home;
	char path[128];
	char epoch_str[epoch_len];
	int fd, count;
	time_t now;
	long epoch;
	ssize_t n;


	home = getenv("HOME");
	if (!home)
		perror("home envp not set");
	snprintf(path, sizeof path, "%s%s", home, ccq_suffix);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		perror("can't open sl");

	for (;;) {
		count = 0;
		now = time(NULL);
		lseek(fd, 0, SEEK_SET);

		for (;;) {
			epoch = 0;
			n = read(fd, epoch_str, epoch_len);
			if (n == 0) /* EOF before epoch */
				break;
			if (n != 10) 
				perror("epoch field read error");

			for (int i = 0; i < epoch_len; ++i) {
				if (epoch_str[i] < '0' || epoch_str[i] > '9')
					fprintf(stderr, "non digit char in epoch\n");
				epoch = epoch * 10 + (epoch_str[i] - '0');
			}

			if (epoch < now)
				++count;

			do {
				n = read(fd, &ch, 1);
				if (n == 0) /* EOF */
					break;
				if (n < 0)
					perror("read error when draining line\n");
			} while (ch != '\n');
		}

		w2s(mname, "%d dues", count);
		sleep(60);
	}

	return 0;
}
