// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int read_long(const char *path, long *value)
{
	FILE *fp = fopen(path, "r");
	int ret;

	if (!fp)
		return -1;
	ret = fscanf(fp, "%ld", value);
	fclose(fp);
	return ret == 1 ? 0 : -1;
}

int main(int argc, char **argv)
{
	char path[PATH_MAX], *end;
	long interval, value;
	struct timespec delay;

	if (argc != 4) {
		fprintf(stderr, "usage: %s HWMON_DIR ATTRIBUTE INTERVAL_MS\\n",
			argv[0]);
		return 2;
	}
	errno = 0;
	interval = strtol(argv[3], &end, 10);
	if (errno || *end || interval < 1 || interval > 3600000) {
		fprintf(stderr, "invalid interval\\n");
		return 2;
	}
	snprintf(path, sizeof(path), "%s/%s", argv[1], argv[2]);
	delay.tv_sec = interval / 1000;
	delay.tv_nsec = (interval % 1000) * 1000000L;
	for (;;) {
		if (read_long(path, &value)) {
			perror(path);
			return 1;
		}
		printf("%ld\\n", value);
		fflush(stdout);
		if (nanosleep(&delay, NULL) && errno != EINTR) {
			perror("nanosleep");
			return 1;
		}
	}
}

