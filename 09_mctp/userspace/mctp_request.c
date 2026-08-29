// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <linux/mctp.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_PAYLOAD 4096

static bool parse_u8(const char *text, int base, uint8_t *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, base);
	if (errno || *end != '\0' || parsed > UINT8_MAX)
		return false;
	*value = (uint8_t)parsed;
	return true;
}

int main(int argc, char **argv)
{
	struct sockaddr_mctp peer = { 0 };
	struct sockaddr_mctp source = { 0 };
	uint8_t tx[MAX_PAYLOAD], rx[MAX_PAYLOAD];
	struct pollfd pfd;
	socklen_t source_len = sizeof(source);
	uint8_t eid, type;
	ssize_t count;
	int fd, i;

	if (argc < 4) {
		fprintf(stderr,
			"usage: %s DEST_EID MESSAGE_TYPE HEX_BYTE [HEX_BYTE ...]\n",
			argv[0]);
		return 2;
	}
	if (!parse_u8(argv[1], 0, &eid) || eid == MCTP_ADDR_NULL ||
	    eid == MCTP_ADDR_ANY || !parse_u8(argv[2], 0, &type)) {
		fprintf(stderr, "invalid EID or message type\n");
		return 2;
	}
	if ((size_t)(argc - 2) > sizeof(tx)) {
		fprintf(stderr, "payload too large\n");
		return 2;
	}
	tx[0] = type;
	for (i = 3; i < argc; i++) {
		if (!parse_u8(argv[i], 16, &tx[i - 2])) {
			fprintf(stderr, "invalid hex byte: %s\n", argv[i]);
			return 2;
		}
	}

	fd = socket(AF_MCTP, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket(AF_MCTP)");
		return 1;
	}
	peer.smctp_family = AF_MCTP;
	peer.smctp_network = MCTP_NET_ANY;
	peer.smctp_addr.s_addr = eid;
	peer.smctp_type = type;
	peer.smctp_tag = MCTP_TAG_OWNER;

	count = sendto(fd, tx, (size_t)(argc - 2), 0,
		       (struct sockaddr *)&peer, sizeof(peer));
	if (count < 0) {
		perror("sendto");
		close(fd);
		return 1;
	}

	pfd.fd = fd;
	pfd.events = POLLIN;
	do {
		count = poll(&pfd, 1, 2000);
	} while (count < 0 && errno == EINTR);
	if (count <= 0) {
		fprintf(stderr, count == 0 ? "response timeout\n" : "poll failed\n");
		close(fd);
		return 1;
	}
	count = recvfrom(fd, rx, sizeof(rx), 0,
			 (struct sockaddr *)&source, &source_len);
	if (count < 0) {
		perror("recvfrom");
		close(fd);
		return 1;
	}
	printf("response: network=%u eid=%u type=0x%02x tag=%u bytes=%zd\n",
	       source.smctp_network, source.smctp_addr.s_addr,
	       source.smctp_type, source.smctp_tag & MCTP_TAG_MASK, count);
	for (i = 0; i < count; i++)
		printf("%s%02x", i ? " " : "", rx[i]);
	putchar('\n');
	close(fd);
	return 0;
}
