// SPDX-License-Identifier: GPL-2.0
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static speed_t baud_to_speed(long baud)
{
	switch (baud) {
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
	case 57600: return B57600;
	case 115200: return B115200;
#ifdef B230400
	case 230400: return B230400;
#endif
	default: return 0;
	}
}

int main(int argc, char **argv)
{
	static const unsigned char pattern[] = {
		0x00, 0x55, 0xaa, 0xff, 'U', 'A', 'R', 'T', '\r', '\n'
	};
	unsigned char rx[sizeof(pattern)];
	struct termios oldtio, tio;
	struct pollfd pfd;
	char *end;
	long baud;
	speed_t speed;
	size_t received = 0;
	ssize_t n;
	int fd, ret = 1;

	if (argc != 3) {
		fprintf(stderr, "usage: %s /dev/ttyX BAUD\n", argv[0]);
		return 2;
	}
	errno = 0;
	baud = strtol(argv[2], &end, 10);
	speed = baud_to_speed(baud);
	if (errno || *end || !speed) {
		fprintf(stderr, "unsupported baud\n");
		return 2;
	}
	fd = open(argv[1], O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	if (tcgetattr(fd, &oldtio)) {
		perror("tcgetattr");
		goto out;
	}
	tio = oldtio;
	cfmakeraw(&tio);
	tio.c_cflag |= CLOCAL | CREAD;
	tio.c_cflag &= ~CRTSCTS;
	cfsetispeed(&tio, speed);
	cfsetospeed(&tio, speed);
	if (tcsetattr(fd, TCSANOW, &tio) || tcflush(fd, TCIOFLUSH)) {
		perror("termios");
		goto restore;
	}
	if (write(fd, pattern, sizeof(pattern)) != sizeof(pattern)) {
		perror("write");
		goto restore;
	}
	tcdrain(fd);
	pfd.fd = fd;
	pfd.events = POLLIN;
	while (received < sizeof(rx)) {
		if (poll(&pfd, 1, 2000) <= 0) {
			fprintf(stderr, "RX timeout\n");
			goto restore;
		}
		n = read(fd, rx + received, sizeof(rx) - received);
		if (n < 0) {
			perror("read");
			goto restore;
		}
		received += n;
	}
	if (memcmp(pattern, rx, sizeof(pattern))) {
		fprintf(stderr, "loopback data mismatch\n");
		goto restore;
	}
	printf("PASS: %zu bytes at %ld baud\n", received, baud);
	ret = 0;
restore:
	if (tcsetattr(fd, TCSANOW, &oldtio))
		perror("restore termios");
out:
	close(fd);
	return ret;
}

