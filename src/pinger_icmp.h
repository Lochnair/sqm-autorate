#ifndef PINGER_ICMP_H
#define PINGER_ICMP_H

#include <stdint.h>

struct icmp_timestamp_hdr
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence;
	uint32_t originateTime;
	uint32_t receiveTime;
	uint32_t transmitTime;
};

int icmp_ping_send(int sock_fd, struct sockaddr_in *reflector, int seq);

void *icmp_receiver_loop(int sock_fd);

#endif // PINGER_ICMP_H