// SPDX-License-Identifier: GPL-2.0
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PECI_DEVICES "/sys/bus/peci/devices"

static const char *base_name(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

static void show_device(const char *name)
{
	char link_path[PATH_MAX], target[PATH_MAX];
	ssize_t length;
	int written;

	written = snprintf(link_path, sizeof(link_path), "%s/%s/driver",
			   PECI_DEVICES, name);
	if (written < 0 || (size_t)written >= sizeof(link_path)) {
		fprintf(stderr, "path too long: %s\n", name);
		return;
	}
	length = readlink(link_path, target, sizeof(target) - 1);
	if (length < 0) {
		if (errno == ENOENT)
			printf("%-24s driver=(unbound)\n", name);
		else
			perror(link_path);
		return;
	}
	target[length] = '\0';
	printf("%-24s driver=%s\n", name, base_name(target));
}

int main(void)
{
	struct dirent **entries;
	int count, i;

	count = scandir(PECI_DEVICES, &entries, NULL, alphasort);
	if (count < 0) {
		if (errno == ENOENT)
			fprintf(stderr, "PECI bus is not present: %s\n", PECI_DEVICES);
		else
			perror("scandir PECI devices");
		return 1;
	}
	printf("PECI devices under %s:\n", PECI_DEVICES);
	for (i = 0; i < count; i++) {
		if (strcmp(entries[i]->d_name, ".") != 0 &&
		    strcmp(entries[i]->d_name, "..") != 0)
			show_device(entries[i]->d_name);
		free(entries[i]);
	}
	free(entries);
	return 0;
}

