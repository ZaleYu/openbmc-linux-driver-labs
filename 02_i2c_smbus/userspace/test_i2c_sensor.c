// SPDX-License-Identifier: MIT
/* Direct I2C_RDWR example for controlled bring-up with no bound kernel driver. */

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEMO_REG_DEVICE_ID 0x00
#define DEMO_REG_TEMP_MSB  0x01
#define DEMO_DEVICE_ID     0xa5

static int read_regs(int fd, uint16_t address, uint8_t reg,
		     uint8_t *data, uint16_t length)
{
	struct i2c_msg messages[2] = {
		{
			.addr = address,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = address,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		},
	};
	struct i2c_rdwr_ioctl_data transfer = {
		.msgs = messages,
		.nmsgs = 2,
	};

	if (ioctl(fd, I2C_RDWR, &transfer) < 0)
		return -1;
	return 0;
}

static long decode_temperature_mc(const uint8_t bytes[2])
{
	int16_t raw = (int16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);

	raw >>= 4;
	return ((long)raw * 625) / 10;
}

static int parse_address(const char *text, uint16_t *address)
{
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(text, &end, 0);
	if (errno || *text == '\0' || *end != '\0' || value > 0x7f)
		return -1;
	*address = (uint16_t)value;
	return 0;
}

int main(int argc, char **argv)
{
	uint8_t id;
	uint8_t temp[2];
	uint16_t address;
	long temperature_mc;
	int fd;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s /dev/i2c-N 0xADDRESS\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (parse_address(argv[2], &address) < 0) {
		fprintf(stderr, "Invalid 7-bit I2C address: %s\n", argv[2]);
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	if (read_regs(fd, address, DEMO_REG_DEVICE_ID, &id, 1) < 0) {
		fprintf(stderr, "Device-ID read failed: %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	if (id != DEMO_DEVICE_ID) {
		fprintf(stderr, "Unexpected device ID 0x%02x (expected 0x%02x)\n",
			id, DEMO_DEVICE_ID);
		close(fd);
		return EXIT_FAILURE;
	}

	if (read_regs(fd, address, DEMO_REG_TEMP_MSB, temp, sizeof(temp)) < 0) {
		fprintf(stderr, "Temperature read failed: %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	temperature_mc = decode_temperature_mc(temp);
	printf("Device ID: 0x%02x\n", id);
	printf("Raw bytes: 0x%02x 0x%02x\n", temp[0], temp[1]);
	printf("Temperature: %s%ld.%03ld C\n",
		temperature_mc < 0 ? "-" : "",
		labs(temperature_mc) / 1000, labs(temperature_mc) % 1000);

	close(fd);
	return EXIT_SUCCESS;
}
