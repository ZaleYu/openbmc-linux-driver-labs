// SPDX-License-Identifier: MIT
/* Perform one combined register read through a logical downstream adapter. */

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_READ_LENGTH 32

static int parse_number(const char *text, unsigned long max,
			unsigned long *value)
{
	char *end;

	errno = 0;
	*value = strtoul(text, &end, 0);
	if (errno || end == text || *end != '\0' || *value > max)
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	struct i2c_rdwr_ioctl_data transaction;
	struct i2c_msg messages[2];
	unsigned char data[MAX_READ_LENGTH];
	unsigned char reg;
	unsigned long address_value, register_value, length_value = 1, i;
	int fd;

	if (argc != 4 && argc != 5) {
		fprintf(stderr,
			"Usage: %s /dev/i2c-N ADDRESS REGISTER [LENGTH]\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	if (parse_number(argv[2], 0x7f, &address_value) ||
	    parse_number(argv[3], 0xff, &register_value) ||
	    (argc == 5 && parse_number(argv[4], MAX_READ_LENGTH,
					 &length_value)) || length_value == 0) {
		fprintf(stderr, "Invalid 7-bit address, 8-bit register, or length 1..32\n");
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	reg = (unsigned char)register_value;
	messages[0].addr = (__u16)address_value;
	messages[0].flags = 0;
	messages[0].len = 1;
	messages[0].buf = &reg;
	messages[1].addr = (__u16)address_value;
	messages[1].flags = I2C_M_RD;
	messages[1].len = (__u16)length_value;
	messages[1].buf = data;
	transaction.msgs = messages;
	transaction.nmsgs = 2;

	if (ioctl(fd, I2C_RDWR, &transaction) < 0) {
		fprintf(stderr, "I2C_RDWR failed: %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	printf("%s address 0x%02lx register 0x%02lx:", argv[1],
	       address_value, register_value);
	for (i = 0; i < length_value; i++)
		printf(" %02x", data[i]);
	putchar('\n');
	close(fd);
	return EXIT_SUCCESS;
}

