/* TODO: include signal handling for graceful exit */

#include <time.h>
#include <unistd.h>

#include "util.h"

static const char *mname = "time";

int
main(void)
{
	for (;;) {
	time_t now = time(NULL);
	struct tm *local_now = localtime(&now);
	char time_str[32];
	strftime(time_str, sizeof time_str, "%r", local_now);
		write_to_slot(mname, "%s", time_str);
		sleep(1);
	}

	return 0;
}
