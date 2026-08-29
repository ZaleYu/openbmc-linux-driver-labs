// SPDX-License-Identifier: MIT
/* Read, and optionally update, the demo driver's standard hwmon attributes. */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int make_path(char *path, size_t size, const char *dir, const char *name)
{
	int length = snprintf(path, size, "%s/%s", dir, name);

	return length < 0 || (size_t)length >= size ? -1 : 0;
}

static int read_attribute(const char *dir, const char *name, int required)
{
	char path[PATH_MAX];
	char value[128];
	FILE *file;

	if (make_path(path, sizeof(path), dir, name))
		return -1;
	file = fopen(path, "r");
	if (!file) {
		if (required)
			fprintf(stderr, "Cannot read %s: %s\n", path, strerror(errno));
		return required ? -1 : 0;
	}
	if (!fgets(value, sizeof(value), file)) {
		fprintf(stderr, "Cannot parse %s\n", path);
		fclose(file);
		return -1;
	}
	fclose(file);
	value[strcspn(value, "\r\n")] = '\0';
	printf("%-16s %s\n", name, value);
	return 0;
}

static int write_temp_max(const char *dir, const char *text)
{
	char path[PATH_MAX];
	char *end;
	long value;
	FILE *file;

	errno = 0;
	value = strtol(text, &end, 10);
	if (errno || end == text || *end != '\0' || value < -128000 || value > 127000) {
		fprintf(stderr, "TEMP_MAX_MILLIC must be -128000..127000\n");
		return -1;
	}
	if (make_path(path, sizeof(path), dir, "temp1_max"))
		return -1;
	file = fopen(path, "w");
	if (!file) {
		fprintf(stderr, "Cannot write %s: %s\n", path, strerror(errno));
		return -1;
	}
	if (fprintf(file, "%ld\n", value) < 0) {
		fprintf(stderr, "Write failed for %s\n", path);
		fclose(file);
		return -1;
	}
	if (fclose(file) != 0) {
		fprintf(stderr, "Close failed for %s: %s\n", path, strerror(errno));
		return -1;
	}
	printf("updated temp1_max to %ld mC\n", value);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Usage: %s /sys/class/hwmon/hwmonN [TEMP_MAX_MILLIC]\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	if (read_attribute(argv[1], "name", 1))
		return EXIT_FAILURE;
	if (argc == 3 && write_temp_max(argv[1], argv[2]))
		return EXIT_FAILURE;
	if (read_attribute(argv[1], "temp1_input", 1) ||
	    read_attribute(argv[1], "temp1_max", 1) ||
	    read_attribute(argv[1], "temp1_alarm", 1))
		return EXIT_FAILURE;
	read_attribute(argv[1], "device/ibi_count", 0);
	read_attribute(argv[1], "device/last_ibi_status", 0);
	return EXIT_SUCCESS;
}
