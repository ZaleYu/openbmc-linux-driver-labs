// SPDX-License-Identifier: MIT
/* Direct GPIO character-device ABI v2 edge monitor. */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/gpio.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested;

static void handle_signal(int signo)
{
	(void)signo;
	stop_requested = 1;
}

static int parse_offset(const char *text, unsigned int *offset)
{
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(text, &end, 0);
	if (errno || *text == '\0' || *end != '\0' || value > UINT_MAX)
		return -1;

	*offset = (unsigned int)value;
	return 0;
}

static int read_value(int request_fd, int *value)
{
	struct gpio_v2_line_values values = {
		.mask = 1,
	};

	if (ioctl(request_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0)
		return -1;

	*value = !!(values.bits & 1);
	return 0;
}

int main(int argc, char **argv)
{
	struct gpio_v2_line_request request = { 0 };
	struct sigaction action = { 0 };
	struct pollfd pfd;
	unsigned int offset;
	int chip_fd;
	int value;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s /dev/gpiochipN OFFSET\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (parse_offset(argv[2], &offset) < 0) {
		fprintf(stderr, "Invalid GPIO line offset: %s\n", argv[2]);
		return EXIT_FAILURE;
	}

	chip_fd = open(argv[1], O_RDONLY | O_CLOEXEC);
	if (chip_fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	request.offsets[0] = offset;
	request.num_lines = 1;
	request.event_buffer_size = 64;
	request.config.flags = GPIO_V2_LINE_FLAG_INPUT |
		GPIO_V2_LINE_FLAG_EDGE_RISING |
		GPIO_V2_LINE_FLAG_EDGE_FALLING;
	strncpy(request.consumer, "demo-gpio-v2-monitor",
		sizeof(request.consumer) - 1);

	if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
		fprintf(stderr, "Cannot request line %u: %s\n", offset, strerror(errno));
		close(chip_fd);
		return EXIT_FAILURE;
	}

	close(chip_fd);

	if (read_value(request.fd, &value) < 0) {
		fprintf(stderr, "Initial value read failed: %s\n", strerror(errno));
		close(request.fd);
		return EXIT_FAILURE;
	}

	printf("Monitoring %s line %u; initial logical value=%d\n",
		argv[1], offset, value);
	printf("Press Ctrl-C to stop.\n");

	action.sa_handler = handle_signal;
	sigemptyset(&action.sa_mask);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	pfd.fd = request.fd;
	pfd.events = POLLIN;

	while (!stop_requested) {
		struct gpio_v2_line_event event;
		ssize_t size;
		int ret;

		ret = poll(&pfd, 1, 1000);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "poll failed: %s\n", strerror(errno));
			close(request.fd);
			return EXIT_FAILURE;
		}
		if (ret == 0)
			continue;

		size = read(request.fd, &event, sizeof(event));
		if (size < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "Event read failed: %s\n", strerror(errno));
			close(request.fd);
			return EXIT_FAILURE;
		}
		if ((size_t)size != sizeof(event)) {
			fprintf(stderr, "Short event read: %zd bytes\n", size);
			close(request.fd);
			return EXIT_FAILURE;
		}

		printf("timestamp=%llu ns edge=%s offset=%u seq=%u line-seq=%u\n",
			(unsigned long long)event.timestamp_ns,
			event.id == GPIO_V2_LINE_EVENT_RISING_EDGE ? "rising" :
			event.id == GPIO_V2_LINE_EVENT_FALLING_EDGE ? "falling" : "unknown",
			event.offset, event.seqno, event.line_seqno);
		fflush(stdout);
	}

	close(request.fd);
	return EXIT_SUCCESS;
}

