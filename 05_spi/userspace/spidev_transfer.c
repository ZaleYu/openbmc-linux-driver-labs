// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	uint8_t tx[256], rx[256] = {0}, mode = SPI_MODE_0;
	uint32_t speed = 500000;
	struct spi_ioc_transfer x = {0};
	char *end;
	int fd, n = 0, i;

	if (argc < 3) {
		fprintf(stderr, "usage: %s /dev/spidevB.C BYTE [BYTE...]\n", argv[0]);
		return 2;
	}
	for (i = 2; i < argc && n < 256; i++) {
		long v = strtol(argv[i], &end, 0);
		if (*end || v < 0 || v > 255) { fprintf(stderr, "bad byte: %s\n", argv[i]); return 2; }
		tx[n++] = (uint8_t)v;
	}
	fd = open(argv[1], O_RDWR);
	if (fd < 0) { perror("open"); return 1; }
	if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) { perror("mode"); close(fd); return 1; }
	x.tx_buf = (uintptr_t)tx; x.rx_buf = (uintptr_t)rx;
	x.len = n; x.speed_hz = speed; x.bits_per_word = 8;
	if (ioctl(fd, SPI_IOC_MESSAGE(1), &x) < 0) { perror("transfer"); close(fd); return 1; }
	for (i = 0; i < n; i++) printf("%02x%c", rx[i], i + 1 == n ? '\n' : ' ');
	close(fd);
	return 0;
}

