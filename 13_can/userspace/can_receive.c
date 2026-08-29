// SPDX-License-Identifier: MIT
#include <errno.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int parse_id(const char *text, canid_t *id)
{
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(text, &end, 16);
	if (errno || *text == '\0' || *end != '\0' || value > CAN_EFF_MASK)
		return -1;
	*id = (canid_t)value;
	return 0;
}

int main(int argc, char **argv)
{
	struct sockaddr_can addr = { .can_family = AF_CAN };
	struct can_filter filter;
	struct can_frame frame;
	canid_t id;
	unsigned int i;
	int fd;
	ssize_t length;

	if (argc != 3) {
		fprintf(stderr, "usage: %s IFACE CAN_ID_HEX\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (parse_id(argv[2], &id) < 0) {
		fprintf(stderr, "invalid CAN ID: %s\n", argv[2]);
		return EXIT_FAILURE;
	}
	if (id > CAN_SFF_MASK) {
		filter.can_id = id | CAN_EFF_FLAG;
		filter.can_mask = CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_EFF_MASK;
	} else {
		filter.can_id = id;
		filter.can_mask = CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_SFF_MASK;
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
	if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER,
		       &filter, sizeof(filter)) < 0) {
		perror("setsockopt CAN_RAW_FILTER");
		close(fd);
		return EXIT_FAILURE;
	}
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(fd);
		return EXIT_FAILURE;
	}

	length = read(fd, &frame, sizeof(frame));
	if (length != (ssize_t)sizeof(frame)) {
		if (length < 0)
			perror("read");
		else
			fprintf(stderr, "short CAN frame: %zd bytes\n", length);
		close(fd);
		return EXIT_FAILURE;
	}
	printf("%s %08X [%u]", argv[1], frame.can_id, frame.len);
	for (i = 0; i < frame.len; i++)
		printf(" %02X", frame.data[i]);
	putchar('\n');
	close(fd);
	return EXIT_SUCCESS;
}
