// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_XFER 4096

static int parse_size(const char *text, size_t *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 0);
	if (errno || *text == '\0' || *end != '\0' || parsed == 0 ||
	    parsed > MAX_XFER)
		return -1;
	*value = (size_t)parsed;
	return 0;
}

static int write_all(int fd, const uint8_t *buf, size_t length)
{
	size_t done = 0;

	while (done < length) {
		ssize_t ret = write(fd, buf + done, length - done);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0)
			return -1;
		done += (size_t)ret;
	}
	return 0;
}

static int read_all(int fd, uint8_t *buf, size_t length)
{
	size_t done = 0;

	while (done < length) {
		struct pollfd pfd = { .fd = fd, .events = POLLIN };
		ssize_t ret;

		ret = poll(&pfd, 1, 3000);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0) {
			errno = ret == 0 ? ETIMEDOUT : errno;
			return -1;
		}
		ret = read(fd, buf + done, length - done);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0)
			return -1;
		done += (size_t)ret;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *path;
	size_t length = 512;
	uint8_t *tx;
	uint8_t *rx;
	size_t i;
	int fd;
	int status = EXIT_FAILURE;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: %s /dev/demo_usbN [1..4096 bytes]\n", argv[0]);
		return EXIT_FAILURE;
	}
	path = argv[1];
	if (argc == 3 && parse_size(argv[2], &length) < 0) {
		fprintf(stderr, "invalid transfer size: %s\n", argv[2]);
		return EXIT_FAILURE;
	}

	tx = malloc(length);
	rx = calloc(length, 1);
	if (!tx || !rx) {
		fprintf(stderr, "allocation failed\n");
		free(tx);
		free(rx);
		return EXIT_FAILURE;
	}
	for (i = 0; i < length; i++)
		tx[i] = (uint8_t)((i * 37U + 0x55U) & 0xffU);

	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", path, strerror(errno));
		goto out_free;
	}
	if (write_all(fd, tx, length) < 0) {
		fprintf(stderr, "bulk write: %s\n", strerror(errno));
		goto out_close;
	}
	if (read_all(fd, rx, length) < 0) {
		fprintf(stderr, "bulk read: %s\n", strerror(errno));
		goto out_close;
	}
	if (memcmp(tx, rx, length) != 0) {
		for (i = 0; i < length && tx[i] == rx[i]; i++)
			;
		fprintf(stderr, "mismatch at byte %zu: sent 0x%02x, got 0x%02x\n",
			i, tx[i], rx[i]);
		goto out_close;
	}

	printf("PASS: %zu-byte bulk loopback on %s\n", length, path);
	status = EXIT_SUCCESS;
out_close:
	close(fd);
out_free:
	free(tx);
	free(rx);
	return status;
}
