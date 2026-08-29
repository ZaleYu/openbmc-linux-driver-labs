// SPDX-License-Identifier: MIT
/* Resolve a Linux I2C mux channel symlink to its logical adapter. */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_number(const char *text, unsigned long max,
			unsigned long *value)
{
	char *end;

	errno = 0;
	*value = strtoul(text, &end, 0);
	if (errno || end == text || *end != '\0' || *value > max)
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	char link_path[PATH_MAX];
	char resolved[PATH_MAX];
	const char *base;
	unsigned long parent, address, channel;
	unsigned int logical_bus;
	int length;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s PARENT_BUS MUX_ADDRESS CHANNEL\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	if (parse_number(argv[1], UINT_MAX, &parent) ||
	    parse_number(argv[2], 0x7f, &address) ||
	    parse_number(argv[3], UINT_MAX, &channel)) {
		fprintf(stderr, "Invalid bus, 7-bit address, or channel\n");
		return EXIT_FAILURE;
	}

	length = snprintf(link_path, sizeof(link_path),
			  "/sys/bus/i2c/devices/%lu-%04lx/channel-%lu",
			  parent, address, channel);
	if (length < 0 || (size_t)length >= sizeof(link_path)) {
		fprintf(stderr, "Generated sysfs path is too long\n");
		return EXIT_FAILURE;
	}
	if (!realpath(link_path, resolved)) {
		fprintf(stderr, "Cannot resolve %s: %s\n", link_path,
			strerror(errno));
		return EXIT_FAILURE;
	}

	base = strrchr(resolved, '/');
	base = base ? base + 1 : resolved;
	if (sscanf(base, "i2c-%u", &logical_bus) != 1) {
		fprintf(stderr, "Unexpected channel target: %s\n", resolved);
		return EXIT_FAILURE;
	}

	printf("mux device:   %lu-%04lx\n", parent, address);
	printf("channel:      %lu\n", channel);
	printf("sysfs target: %s\n", resolved);
	printf("logical bus:  %u\n", logical_bus);
	printf("device node:  /dev/i2c-%u\n", logical_bus);
	return EXIT_SUCCESS;
}

