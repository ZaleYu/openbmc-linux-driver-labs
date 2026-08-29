// SPDX-License-Identifier: MIT
/* Print common read-only attributes for one Linux I3C sysfs device. */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_attribute(const char *directory, const char *name)
{
	char path[PATH_MAX];
	char value[256];
	FILE *file;
	int length;

	length = snprintf(path, sizeof(path), "%s/%s", directory, name);
	if (length < 0 || (size_t)length >= sizeof(path))
		return;

	file = fopen(path, "r");
	if (!file)
		return;
	if (fgets(value, sizeof(value), file)) {
		value[strcspn(value, "\r\n")] = '\0';
		printf("%-20s %s\n", name, value);
	}
	fclose(file);
}

int main(int argc, char **argv)
{
	static const char * const attributes[] = {
		"pid", "bcr", "dcr", "dynamic_address", "hdrcap", "modalias",
		"ibi_count", "last_ibi_status", "uevent",
	};
	char driver_path[PATH_MAX];
	char target[PATH_MAX];
	ssize_t target_length;
	size_t i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s /sys/bus/i3c/devices/DEVICE\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (access(argv[1], R_OK | X_OK) != 0) {
		fprintf(stderr, "Cannot access %s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	printf("device               %s\n", argv[1]);
	for (i = 0; i < sizeof(attributes) / sizeof(attributes[0]); i++)
		print_attribute(argv[1], attributes[i]);

	if (snprintf(driver_path, sizeof(driver_path), "%s/driver", argv[1]) > 0) {
		target_length = readlink(driver_path, target, sizeof(target) - 1);
		if (target_length >= 0) {
			target[target_length] = '\0';
			printf("%-20s %s\n", "driver", target);
		} else {
			printf("%-20s unbound\n", "driver");
		}
	}

	return EXIT_SUCCESS;
}

