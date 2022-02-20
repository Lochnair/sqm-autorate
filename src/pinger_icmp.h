#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <asm/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <linux/net_tstamp.h>
#include <linux/time_types.h>

struct sockaddr_in;

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