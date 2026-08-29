// SPDX-License-Identifier: GPL-2.0
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/ipmi_bmc.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define KCS_MESSAGE_MAX 1000
#define IPMI_CC_INVALID_COMMAND 0xc1

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number)
{
	(void)signal_number;
	stop_requested = 1;
}

static bool valid_device_path(const char *path)
{
	static const char prefix[] = "/dev/ipmi-kcs";
	const unsigned char *cursor;

	if (strncmp(path, prefix, sizeof(prefix) - 1) != 0)
		return false;
	cursor = (const unsigned char *)path + sizeof(prefix) - 1;
	if (!*cursor)
		return false;
	for (; *cursor; cursor++)
		if (!isdigit(*cursor))
			return false;
	return true;
}

static void dump_request(const uint8_t *request, ssize_t length)
{
	ssize_t i;

	printf("request bytes=%zd:", length);
	for (i = 0; i < length; i++)
		printf(" %02x", request[i]);
	putchar('\n');
	fflush(stdout);
}

int main(int argc, char **argv)
{
	uint8_t request[KCS_MESSAGE_MAX], response[3];
	struct sigaction action = { 0 };
	struct pollfd pfd;
	ssize_t length, written;
	unsigned int netfn;
	int fd, ready;

	if (argc != 3 || strcmp(argv[1], "--lab") != 0 ||
	    !valid_device_path(argv[2])) {
		fprintf(stderr, "usage: %s --lab /dev/ipmi-kcsN\n", argv[0]);
		return 2;
	}
	fprintf(stderr,
		"LAB ONLY: stop kcsbridge first; all requests receive completion 0xC1.\n");
	fd = open(argv[2], O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		perror("open KCS device");
		return 1;
	}
	action.sa_handler = request_stop;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) < 0 ||
	    sigaction(SIGTERM, &action, NULL) < 0) {
		perror("sigaction");
		close(fd);
		return 1;
	}
	pfd.fd = fd;
	pfd.events = POLLIN;

	while (!stop_requested) {
		ready = poll(&pfd, 1, 1000);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			break;
		}
		if (ready == 0)
			continue;
		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "KCS device poll error: %#x\n", pfd.revents);
			break;
		}
		if (!(pfd.revents & POLLIN))
			continue;
		length = read(fd, request, sizeof(request));
		if (length < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			perror("read request");
			break;
		}
		dump_request(request, length);
		if (length < 2) {
			fprintf(stderr, "short IPMI request; forcing KCS abort\n");
			if (ioctl(fd, IPMI_BMC_IOCTL_FORCE_ABORT) < 0)
				perror("force abort");
			continue;
		}

		netfn = (request[0] >> 2) & 0x3f;
		response[0] = (uint8_t)(((netfn | 1U) << 2) |
					(request[0] & 0x03));
		response[1] = request[1];
		response[2] = IPMI_CC_INVALID_COMMAND;
		written = write(fd, response, sizeof(response));
		if (written < 0) {
			perror("write response");
			break;
		}
		if ((size_t)written != sizeof(response)) {
			fprintf(stderr, "short KCS response write\n");
			break;
		}
	}
	close(fd);
	return 0;
}
