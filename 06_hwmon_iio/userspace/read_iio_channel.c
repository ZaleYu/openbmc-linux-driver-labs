// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int read_number(const char *path, double *value)
{
	FILE *fp = fopen(path, "r");
	char buf[128], *end;

	if (!fp)
		return -1;
	if (!fgets(buf, sizeof(buf), fp)) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	errno = 0;
	*value = strtod(buf, &end);
	if (errno || end == buf)
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	char path[PATH_MAX];
	double raw, scale, offset = 0.0;
	char *end;
	long channel;

	if (argc != 3) {
		fprintf(stderr, "usage: %s IIO_DEVICE VOLTAGE_CHANNEL\\n", argv[0]);
		return 2;
	}
	errno = 0;
	channel = strtol(argv[2], &end, 10);
	if (errno || *end || channel < 0 || channel > 255) {
		fprintf(stderr, "invalid channel\\n");
		return 2;
	}
	snprintf(path, sizeof(path), "%s/in_voltage%ld_raw", argv[1], channel);
	if (read_number(path, &raw)) {
		perror(path);
		return 1;
	}
	snprintf(path, sizeof(path), "%s/in_voltage_scale", argv[1]);
	if (read_number(path, &scale)) {
		snprintf(path, sizeof(path), "%s/in_voltage%ld_scale",
			 argv[1], channel);
		if (read_number(path, &scale)) {
			perror(path);
			return 1;
		}
	}
	snprintf(path, sizeof(path), "%s/in_voltage%ld_offset", argv[1], channel);
	(void)read_number(path, &offset);
	printf("raw=%.0f scale=%g offset=%g processed=%g mV\\n",
	       raw, scale, offset, (raw + offset) * scale);
	return 0;
}

