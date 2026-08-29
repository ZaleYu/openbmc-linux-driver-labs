// SPDX-License-Identifier: MIT
#include <errno.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int parse_u32(const char *text, unsigned long max, unsigned long *value)
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

int main(int argc, char **argv)
{
	struct sockaddr_can addr = { .can_family = AF_CAN };
	struct can_frame frame = { 0 };
	unsigned long id;
	int fd;
	int i;

	if (argc < 3 || argc > 11) {
		fprintf(stderr, "usage: %s IFACE CAN_ID_HEX [BYTE_HEX ... up to 8]\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	if (parse_u32(argv[2], CAN_EFF_MASK, &id) < 0) {
		fprintf(stderr, "invalid CAN ID: %s\n", argv[2]);
		return EXIT_FAILURE;
	}
	frame.can_id = (canid_t)id;
	if (id > CAN_SFF_MASK)
		frame.can_id |= CAN_EFF_FLAG;
	frame.len = (unsigned char)(argc - 3);
	for (i = 3; i < argc; i++) {
		unsigned long byte;

		if (parse_u32(argv[i], 0xff, &byte) < 0) {
			fprintf(stderr, "invalid data byte: %s\n", argv[i]);
			return EXIT_FAILURE;
		}
		frame.data[i - 3] = (uint8_t)byte;
	}

	addr.can_ifindex = (int)if_nametoindex(argv[1]);
	if (!addr.can_ifindex) {
		fprintf(stderr, "interface %s: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}
	fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (fd < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(fd);
		return EXIT_FAILURE;
	}
	if (write(fd, &frame, sizeof(frame)) != (ssize_t)sizeof(frame)) {
		perror("write");
		close(fd);
		return EXIT_FAILURE;
	}

	printf("sent %03lX [%u] on %s\n", id, frame.len, argv[1]);
	close(fd);
	return EXIT_SUCCESS;
}
