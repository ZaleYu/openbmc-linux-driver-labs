// SPDX-License-Identifier: GPL-2.0
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested;

static void on_signal(int signo)
{
	(void)signo;
	stop_requested = 1;
}

static speed_t baud_to_speed(long baud)
{
	switch (baud) {
	case 9600: return B9600;
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
	unsigned char buf[256];
	struct termios oldtio, tio;
	struct pollfd pfd;
	struct timespec ts;
	struct tm tm;
	char stamp[32], *end;
	long baud;
	speed_t speed;
	ssize_t n;
	int fd, i, ret = 1;

	if (argc != 3) {
		fprintf(stderr, "usage: %s /dev/ttyX BAUD\n", argv[0]);
		return 2;
	}
	errno = 0;
	baud = strtol(argv[2], &end, 10);
	speed = baud_to_speed(baud);
	if (errno || *end || !speed)
		return 2;
	fd = open(argv[1], O_RDONLY | O_NOCTTY | O_NONBLOCK);
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
	if (tcsetattr(fd, TCSANOW, &tio)) {
		perror("tcsetattr");
		goto restore;
	}
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	pfd.fd = fd;
	pfd.events = POLLIN;
	while (!stop_requested) {
		if (poll(&pfd, 1, 1000) <= 0)
			continue;
		n = read(fd, buf, sizeof(buf));
		if (n < 0 && errno == EAGAIN)
			continue;
		if (n <= 0)
			break;
		clock_gettime(CLOCK_REALTIME, &ts);
		gmtime_r(&ts.tv_sec, &tm);
		strftime(stamp, sizeof(stamp), "%FT%T", &tm);
		printf("%s.%03ldZ", stamp, ts.tv_nsec / 1000000);
		for (i = 0; i < n; i++)
			printf(" %02x", buf[i]);
		putchar('\n');
		fflush(stdout);
	}
	ret = 0;
restore:
	if (tcsetattr(fd, TCSANOW, &oldtio))
		perror("restore termios");
out:
	close(fd);
	return ret;
}

