// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_text(const char *dir, const char *name, char *buf, size_t size)
{
	char path[PATH_MAX];
	FILE *fp;

	if (snprintf(path, sizeof(path), "%s/%s", dir, name) >= (int)sizeof(path))
		return -1;
	fp = fopen(path, "r");
	if (!fp)
		return -1;
	if (!fgets(buf, (int)size, fp)) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	buf[strcspn(buf, "\r\n")] = '\0';
	return 0;
}

static void value_or_dash(const char *dir, const char *name,
			  char *buf, size_t size)
{
	if (read_text(dir, name, buf, size) < 0)
		snprintf(buf, size, "-");
}

int main(void)
{
	const char *root = "/sys/bus/usb/devices";
	struct dirent *entry;
	DIR *dp = opendir(root);

	if (!dp) {
		fprintf(stderr, "cannot open %s: %s\n", root, strerror(errno));
		return EXIT_FAILURE;
	}

	printf("%-12s %-9s %-5s %-5s %-8s %s\n",
	       "sysfs", "VID:PID", "bus", "dev", "speed", "product");
	while ((entry = readdir(dp)) != NULL) {
		char path[PATH_MAX];
		char vid[32], pid[32], bus[32], dev[32], speed[32], product[256];

		if (entry->d_name[0] == '.')
			continue;
		if (snprintf(path, sizeof(path), "%s/%s", root, entry->d_name) >=
		    (int)sizeof(path))
			continue;
		if (read_text(path, "idVendor", vid, sizeof(vid)) < 0)
			continue;
		value_or_dash(path, "idProduct", pid, sizeof(pid));
		value_or_dash(path, "busnum", bus, sizeof(bus));
		value_or_dash(path, "devnum", dev, sizeof(dev));
		value_or_dash(path, "speed", speed, sizeof(speed));
		value_or_dash(path, "product", product, sizeof(product));
		printf("%-12s %s:%-4s %-5s %-5s %-8s %s\n",
		       entry->d_name, vid, pid, bus, dev, speed, product);
	}

	closedir(dp);
	return EXIT_SUCCESS;
}
