// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

static const char *base_name(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

static void show_device(const char *device)
{
	char sysfs[PATH_MAX], target[PATH_MAX];
	struct stat status;
	ssize_t length;
	int written;

	if (stat(device, &status) < 0) {
		perror(device);
		return;
	}
	printf("%s type=%s major=%u minor=%u",
	       device, S_ISCHR(status.st_mode) ? "char" : "unexpected",
	       major(status.st_rdev), minor(status.st_rdev));

	written = snprintf(sysfs, sizeof(sysfs), "/sys/class/misc/%s/device/driver",
			   base_name(device));
	if (written < 0 || (size_t)written >= sizeof(sysfs)) {
		puts(" driver=(path-too-long)");
		return;
	}
	length = readlink(sysfs, target, sizeof(target) - 1);
	if (length < 0) {
		puts(errno == ENOENT ? " driver=(unbound)" : " driver=(unknown)");
		return;
	}
	target[length] = '\0';
	printf(" driver=%s\n", base_name(target));
}

int main(void)
{
	glob_t matches;
	int result;
	size_t i;

	result = glob("/dev/ipmi-kcs*", 0, NULL, &matches);
	if (result == GLOB_NOMATCH) {
		fprintf(stderr, "no /dev/ipmi-kcs* devices found\n");
		return 1;
	}
	if (result != 0) {
		fprintf(stderr, "glob failed: %d\n", result);
		return 1;
	}
	for (i = 0; i < matches.gl_pathc; i++)
		show_device(matches.gl_pathv[i]);
	globfree(&matches);
	return 0;
}

