// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <linux/mctp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_PAYLOAD 4096

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number)
{
	(void)signal_number;
	stop_requested = 1;
}

static bool parse_u8(const char *text, uint8_t *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 0);
	if (errno || *end != '\0' || parsed > UINT8_MAX)
		return false;
	*value = (uint8_t)parsed;
	return true;
}

int main(int argc, char **argv)
{
	struct sockaddr_mctp local = { 0 };
	struct sockaddr_mctp peer;
	struct sigaction action = { 0 };
	uint8_t buffer[MAX_PAYLOAD], type;
	socklen_t peer_len;
	ssize_t count, written;
	int fd;

	if (argc != 2 || !parse_u8(argv[1], &type)) {
		fprintf(stderr, "usage: %s MESSAGE_TYPE\n", argv[0]);
		return 2;
	}
	fd = socket(AF_MCTP, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket(AF_MCTP)");
		return 1;
	}
	local.smctp_family = AF_MCTP;
	local.smctp_network = MCTP_NET_ANY;
	local.smctp_addr.s_addr = MCTP_ADDR_ANY;
	local.smctp_type = type;
	local.smctp_tag = MCTP_TAG_OWNER;
	if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
		perror("bind");
		close(fd);
		return 1;
	}
	action.sa_handler = request_stop;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) < 0 ||
	    sigaction(SIGTERM, &action, NULL) < 0) {
		perror("sigaction");
		close(fd);
		return 1;
	}
	printf("echo responder listening for MCTP type 0x%02x\n", type);

	while (!stop_requested) {
		peer_len = sizeof(peer);
		count = recvfrom(fd, buffer, sizeof(buffer), 0,
				 (struct sockaddr *)&peer, &peer_len);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			perror("recvfrom");
			break;
		}
		printf("request: network=%u eid=%u tag=%u bytes=%zd\n",
		       peer.smctp_network, peer.smctp_addr.s_addr,
		       peer.smctp_tag & MCTP_TAG_MASK, count);

		/* A response uses the request tag with the owner bit cleared. */
		peer.smctp_tag &= ~MCTP_TAG_OWNER;
		written = sendto(fd, buffer, (size_t)count, 0,
				 (struct sockaddr *)&peer, peer_len);
		if (written < 0)
			perror("sendto");
		else if (written != count)
			fprintf(stderr, "short datagram write\n");
	}
	close(fd);
	return 0;
}
