// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int read_regs(int fd, uint8_t reg, uint8_t *out, size_t n)
{
	uint8_t cmd = 0x80u | reg;
	struct spi_ioc_transfer x[2] = {
		{ .tx_buf = (uintptr_t)&cmd, .len = 1 },
		{ .rx_buf = (uintptr_t)out, .len = n },
	};
	return ioctl(fd, SPI_IOC_MESSAGE(2), x);
}

int main(int argc, char **argv)
{
	uint32_t speed = 1000000;
	uint8_t mode = SPI_MODE_0, id, raw[2], status;
	int16_t temp;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s /dev/spidevB.C\n", argv[0]);
		return 2;
	}
	fd = open(argv[1], O_RDWR);
	if (fd < 0) { perror("open"); return 1; }
	if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
	    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
		perror("SPI setup"); close(fd); return 1;
	}
	if (read_regs(fd, 0x00, &id, 1) < 0 ||
	    read_regs(fd, 0x01, raw, 2) < 0 ||
	    read_regs(fd, 0x03, &status, 1) < 0) {
		perror("SPI_IOC_MESSAGE"); close(fd); return 1;
	}
	temp = (int16_t)((uint16_t)raw[0] << 8 | raw[1]);
	printf("id=0x%02x temperature=%.2f C alarm=%u\n",
	       id, temp / 100.0, status & 1u);
	close(fd);
	return id == 0x5a ? 0 : 3;
}

