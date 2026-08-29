// SPDX-License-Identifier: MIT
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static uint8_t make_pid(uint8_t id)
{
	uint8_t p0 = ((id >> 0) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4)) & 1U;
	uint8_t p1 = (~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5))) & 1U;

	return (uint8_t)(id | (p0 << 6) | (p1 << 7));
}

static uint8_t lin_checksum(uint8_t seed, const uint8_t *data, size_t count)
{
	unsigned int sum = seed;
	size_t i;

	for (i = 0; i < count; i++) {
		sum += data[i];
		if (sum > 0xffU)
			sum = (sum & 0xffU) + 1U;
	}
	return (uint8_t)~sum;
}

static int parse_number(const char *text, int base, unsigned long max,
			unsigned long *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, base);
	if (errno || *text == '\0' || *end != '\0' || parsed > max)
		return -1;
	*value = parsed;
	return 0;
}

static int write_all(int fd, const uint8_t *data, size_t count)
{
	size_t done = 0;

	while (done < count) {
		ssize_t ret = write(fd, data + done, count - done);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0)
			return -1;
		done += (size_t)ret;
	}
	return 0;
}

static int read_exact(int fd, uint8_t *data, size_t count, int timeout_ms)
{
	size_t done = 0;

	while (done < count) {
		struct pollfd pfd = { .fd = fd, .events = POLLIN };
		int ready = poll(&pfd, 1, timeout_ms);
		ssize_t ret;

		if (ready < 0 && errno == EINTR)
			continue;
		if (ready == 0) {
			errno = ETIMEDOUT;
			return -1;
		}
		if (ready < 0)
			return -1;
		ret = read(fd, data + done, count - done);
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
	struct timespec break_time = { .tv_sec = 0, .tv_nsec = 800000 };
	struct termios tty;
	uint8_t response[9];
	uint8_t header[2];
	uint8_t first_two[2];
	unsigned long id_value, data_len_value, timeout_value = 100;
	uint8_t expected;
	bool header_echo = false;
	size_t i;
	int fd;

	if (argc < 4 || argc > 5 ||
	    parse_number(argv[2], 16, 0x3f, &id_value) < 0 ||
	    parse_number(argv[3], 10, 8, &data_len_value) < 0 || data_len_value == 0 ||
	    (argc == 5 && parse_number(argv[4], 10, 5000, &timeout_value) < 0)) {
		fprintf(stderr, "usage: %s TTY ID_HEX DATA_LEN_1_TO_8 [TIMEOUT_MS]\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}
	if (tcgetattr(fd, &tty) < 0) {
		perror("tcgetattr");
		goto fail;
	}
	cfmakeraw(&tty);
	tty.c_cflag = (tty.c_cflag & ~(CSIZE | PARENB | CSTOPB | CRTSCTS)) |
		      CS8 | CLOCAL | CREAD;
	if (cfsetispeed(&tty, B19200) < 0 || cfsetospeed(&tty, B19200) < 0 ||
	    tcsetattr(fd, TCSANOW, &tty) < 0) {
		perror("configure 19200 8N1");
		goto fail;
	}
	if (tcflush(fd, TCIOFLUSH) < 0) {
		perror("tcflush");
		goto fail;
	}

	header[0] = 0x55;
	header[1] = make_pid((uint8_t)id_value);
	if (ioctl(fd, TIOCSBRK) < 0) {
		perror("TIOCSBRK");
		goto fail;
	}
	if (nanosleep(&break_time, NULL) < 0) {
		perror("nanosleep");
		(void)ioctl(fd, TIOCCBRK);
		goto fail;
	}
	if (ioctl(fd, TIOCCBRK) < 0) {
		perror("TIOCCBRK");
		goto fail;
	}
	if (write_all(fd, header, sizeof(header)) < 0 || tcdrain(fd) < 0) {
		perror("write LIN header");
		goto fail;
	}
	if (read_exact(fd, first_two, sizeof(first_two), (int)timeout_value) < 0) {
		perror("read LIN response");
		goto fail;
	}
	if (first_two[0] == header[0] && first_two[1] == header[1]) {
		header_echo = true;
		if (read_exact(fd, response, (size_t)data_len_value + 1U,
			       (int)timeout_value) < 0) {
			perror("read LIN response after header echo");
			goto fail;
		}
	} else {
		response[0] = first_two[0];
		response[1] = first_two[1];
		if (data_len_value > 1 &&
		    read_exact(fd, response + 2, (size_t)data_len_value - 1U,
			       (int)timeout_value) < 0) {
			perror("read remaining LIN response");
			goto fail;
		}
	}

	expected = lin_checksum((id_value == 0x3c || id_value == 0x3d) ? 0 : header[1],
				response, (size_t)data_len_value);
	printf("ID=0x%02lX PID=0x%02X data=", id_value, header[1]);
	for (i = 0; i < (size_t)data_len_value; i++)
		printf("%s%02X", i ? " " : "", response[i]);
	printf(" checksum=0x%02X (%s), header-echo=%s\n", response[data_len_value],
	       response[data_len_value] == expected ? "valid" : "INVALID",
	       header_echo ? "yes" : "no");
	close(fd);
	return response[data_len_value] == expected ? EXIT_SUCCESS : EXIT_FAILURE;

fail:
	close(fd);
	return EXIT_FAILURE;
}
