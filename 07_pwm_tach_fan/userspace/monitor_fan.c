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
	char fan_path[PATH_MAX], pwm_path[PATH_MAX], *end;
	struct timespec delay;
	long interval, rpm, pwm;

	if (argc != 3) {
		fprintf(stderr, "usage: %s HWMON_DIR INTERVAL_MS\n", argv[0]);
		return 2;
	}
	errno = 0;
	interval = strtol(argv[2], &end, 10);
	if (errno || *end || interval < 10 || interval > 3600000) {
		fprintf(stderr, "invalid interval\n");
		return 2;
	}
	snprintf(fan_path, sizeof(fan_path), "%s/fan1_input", argv[1]);
	snprintf(pwm_path, sizeof(pwm_path), "%s/pwm1", argv[1]);
	delay.tv_sec = interval / 1000;
	delay.tv_nsec = interval % 1000 * 1000000L;

	for (;;) {
		if (read_long(fan_path, &rpm) || read_long(pwm_path, &pwm)) {
			perror("read hwmon");
			return 1;
		}
		printf("pwm=%ld rpm=%ld\n", pwm, rpm);
		fflush(stdout);
		if (nanosleep(&delay, NULL) && errno != EINTR) {
			perror("nanosleep");
			return 1;
		}
	}
}

