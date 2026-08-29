// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile sig_atomic_t stop_requested;

static void on_signal(int signo)
{
	(void)signo;
	stop_requested = 1;
}

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

static int write_long(const char *path, long value)
{
	FILE *fp = fopen(path, "w");
	int ret;

	if (!fp)
		return -1;
	ret = fprintf(fp, "%ld\n", value) < 0 ? -1 : 0;
	if (fclose(fp))
		ret = -1;
	return ret;
}

int main(int argc, char **argv)
{
	char pwm_path[PATH_MAX], rpm_path[PATH_MAX], *end;
	const char *temp_path;
	struct timespec delay;
	long min_pwm, step, dwell_ms, max_temp, temp, rpm, pwm;
	int status = 1;

	if (argc != 7) {
		fprintf(stderr,
			"usage: %s HWMON_DIR MIN_PWM STEP DWELL_MS TEMP_FILE MAX_TEMP_MC\n",
			argv[0]);
		return 2;
	}
	errno = 0;
	min_pwm = strtol(argv[2], &end, 10);
	if (errno || *end || min_pwm < 0 || min_pwm > 255)
		return 2;
	step = strtol(argv[3], &end, 10);
	if (*end || step < 1 || step > 255)
		return 2;
	dwell_ms = strtol(argv[4], &end, 10);
	if (*end || dwell_ms < 100 || dwell_ms > 600000)
		return 2;
	temp_path = argv[5];
	max_temp = strtol(argv[6], &end, 10);
	if (*end)
		return 2;

	snprintf(pwm_path, sizeof(pwm_path), "%s/pwm1", argv[1]);
	snprintf(rpm_path, sizeof(rpm_path), "%s/fan1_input", argv[1]);
	delay.tv_sec = dwell_ms / 1000;
	delay.tv_nsec = dwell_ms % 1000 * 1000000L;
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	for (pwm = 255; pwm >= min_pwm && !stop_requested; pwm -= step) {
		if (write_long(pwm_path, pwm))
			goto out;
		if (nanosleep(&delay, NULL) && errno != EINTR)
			goto out;
		if (read_long(temp_path, &temp) || read_long(rpm_path, &rpm))
			goto out;
		printf("pwm=%ld rpm=%ld temp_mc=%ld\n", pwm, rpm, temp);
		if (temp >= max_temp) {
			fprintf(stderr, "temperature guard reached\n");
			goto out;
		}
		if (pwm < step)
			break;
	}
	status = stop_requested ? 130 : 0;
out:
	if (write_long(pwm_path, 255))
		fprintf(stderr, "WARNING: failed to restore full speed\n");
	return status;
}
