// SPDX-License-Identifier: MIT
/* Direct GPIO character-device ABI v2 timed output controller. */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/gpio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int parse_unsigned(const char *text, unsigned int maximum,
			  unsigned int *result)
{
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(text, &end, 0);
	if (errno || *text == '\0' || *end != '\0' || value > maximum)
		return -1;

	*result = (unsigned int)value;
	return 0;
}

int main(int argc, char **argv)
{
	struct gpio_v2_line_request request = { 0 };
	unsigned int active_low = 0;
	unsigned int duration;
	unsigned int offset;
	unsigned int value;
	int chip_fd;

	if (argc != 5 && argc != 6) {
		fprintf(stderr,
			"Usage: %s /dev/gpiochipN OFFSET VALUE HOLD_SECONDS [active-low]\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	if (parse_unsigned(argv[2], UINT_MAX, &offset) < 0 ||
	    parse_unsigned(argv[3], 1, &value) < 0 ||
	    parse_unsigned(argv[4], 86400, &duration) < 0) {
		fprintf(stderr, "Invalid offset, value, or hold duration\n");
		return EXIT_FAILURE;
	}

	if (argc == 6) {
		if (strcmp(argv[5], "active-low") != 0) {
			fprintf(stderr, "The optional argument must be 'active-low'\n");
			return EXIT_FAILURE;
		}
		active_low = 1;
	}

	chip_fd = open(argv[1], O_RDONLY | O_CLOEXEC);
	if (chip_fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	request.offsets[0] = offset;
	request.num_lines = 1;
	request.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
	if (active_low)
		request.config.flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;

	/* Configure the initial logical value as part of the atomic request. */
	request.config.num_attrs = 1;
	request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
	request.config.attrs[0].attr.values = value ? 1 : 0;
	request.config.attrs[0].mask = 1;
	strncpy(request.consumer, "demo-gpio-v2-output",
		sizeof(request.consumer) - 1);

	if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
		fprintf(stderr, "Cannot request line %u: %s\n", offset, strerror(errno));
		close(chip_fd);
		return EXIT_FAILURE;
	}

	close(chip_fd);
	printf("Holding line %u at logical value %u for %u seconds%s.\n",
		offset, value, duration, active_low ? " (active-low)" : "");
	printf("The output state after this process exits is not guaranteed.\n");
	sleep(duration);
	close(request.fd);

	return EXIT_SUCCESS;
}

