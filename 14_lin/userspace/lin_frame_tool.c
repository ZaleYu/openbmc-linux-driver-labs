// SPDX-License-Identifier: MIT
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_hex(const char *text, unsigned long max, unsigned long *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 16);
	if (errno || *text == '\0' || *end != '\0' || parsed > max)
		return -1;
	*value = parsed;
	return 0;
}

static uint8_t make_pid(uint8_t id)
{
	uint8_t p0 = ((id >> 0) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4)) & 1U;
	uint8_t p1 = (~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5))) & 1U;

	return (uint8_t)((id & 0x3fU) | (p0 << 6) | (p1 << 7));
}

static uint8_t checksum(uint8_t seed, const uint8_t *data, size_t count)
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

static void usage(const char *program)
{
	fprintf(stderr,
		"usage:\n  %s pid ID_HEX\n"
		"  %s checksum classic|enhanced ID_HEX [BYTE_HEX ... up to 8]\n",
		program, program);
}

int main(int argc, char **argv)
{
	unsigned long id_value;
	uint8_t id;
	uint8_t pid;
	uint8_t data[8];
	uint8_t seed;
	int i;

	if (argc == 3 && strcmp(argv[1], "pid") == 0) {
		if (parse_hex(argv[2], 0x3f, &id_value) < 0) {
			fprintf(stderr, "invalid six-bit ID: %s\n", argv[2]);
			return EXIT_FAILURE;
		}
		printf("ID=0x%02lX PID=0x%02X\n", id_value,
		       make_pid((uint8_t)id_value));
		return EXIT_SUCCESS;
	}
	if (argc < 4 || argc > 12 || strcmp(argv[1], "checksum") != 0 ||
	    (strcmp(argv[2], "classic") != 0 &&
	     strcmp(argv[2], "enhanced") != 0)) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (parse_hex(argv[3], 0x3f, &id_value) < 0) {
		fprintf(stderr, "invalid six-bit ID: %s\n", argv[3]);
		return EXIT_FAILURE;
	}
	id = (uint8_t)id_value;
	pid = make_pid(id);
	for (i = 4; i < argc; i++) {
		unsigned long byte;

		if (parse_hex(argv[i], 0xff, &byte) < 0) {
			fprintf(stderr, "invalid byte: %s\n", argv[i]);
			return EXIT_FAILURE;
		}
		data[i - 4] = (uint8_t)byte;
	}
	seed = strcmp(argv[2], "enhanced") == 0 ? pid : 0;
	if ((id == 0x3c || id == 0x3d) && seed != 0)
		fprintf(stderr, "warning: LIN diagnostic IDs normally use classic checksum\n");
	printf("ID=0x%02X PID=0x%02X %s-checksum=0x%02X\n",
	       id, pid, argv[2], checksum(seed, data, (size_t)(argc - 4)));
	return EXIT_SUCCESS;
}
