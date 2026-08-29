// SPDX-License-Identifier: MIT
/* SMBus byte-data example for controlled bring-up with no bound kernel driver. */

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
#define DEMO_REG_STATUS    0x03
#define DEMO_REG_TEMP_HIGH 0x05
#define DEMO_DEVICE_ID     0xa5

static int smbus_read_byte_data(int fd, uint8_t command)
{
	union i2c_smbus_data data;
	struct i2c_smbus_ioctl_data args = {
		.read_write = I2C_SMBUS_READ,
		.command = command,
		.size = I2C_SMBUS_BYTE_DATA,
		.data = &data,
	};

	if (ioctl(fd, I2C_SMBUS, &args) < 0)
		return -1;
	return data.byte & 0xff;
}

static int parse_address(const char *text, int *address)
{
	char *end;
	long value;

	errno = 0;
	value = strtol(text, &end, 0);
	if (errno || *text == '\0' || *end != '\0' || value < 0 || value > 0x7f)
		return -1;
	*address = (int)value;
	return 0;
}

int main(int argc, char **argv)
{
	unsigned long funcs;
	int address;
	int fd;
	int id;
	int status;
	int threshold;

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

	if (ioctl(fd, I2C_FUNCS, &funcs) < 0) {
		fprintf(stderr, "I2C_FUNCS failed: %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	if (!(funcs & I2C_FUNC_SMBUS_BYTE_DATA)) {
		fprintf(stderr, "Adapter does not support SMBus byte-data operations\n");
		close(fd);
		return EXIT_FAILURE;
	}

	if (ioctl(fd, I2C_SLAVE, address) < 0) {
		fprintf(stderr, "Cannot select address 0x%02x: %s\n",
			address, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	id = smbus_read_byte_data(fd, DEMO_REG_DEVICE_ID);
	status = smbus_read_byte_data(fd, DEMO_REG_STATUS);
	threshold = smbus_read_byte_data(fd, DEMO_REG_TEMP_HIGH);
	if (id < 0 || status < 0 || threshold < 0) {
		fprintf(stderr, "SMBus register read failed: %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	printf("Adapter functions: 0x%08lx\n", funcs);
	printf("Device ID: 0x%02x%s\n", id,
		id == DEMO_DEVICE_ID ? "" : " (unexpected)");
	printf("Status: 0x%02x (high alarm: %s)\n",
		status, (status & 0x01) ? "yes" : "no");
	printf("High threshold: %d C\n", (int8_t)threshold);

	close(fd);
	return id == DEMO_DEVICE_ID ? EXIT_SUCCESS : EXIT_FAILURE;
}
