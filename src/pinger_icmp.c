#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/time_types.h>

#include "pinger_icmp.h"
#include "utils.h"

int icmp_ping_send(int sock_fd, struct sockaddr_in *reflector, int seq)
{
	struct icmp_timestamp_hdr hdr;

	memset(&hdr, 0, sizeof(hdr));

	hdr.type = ICMP_TIMESTAMP;
	hdr.identifier = htons(0xFEED);
	hdr.sequence = seq;
	hdr.originateTime = htonl(get_time_since_midnight_ms());

	hdr.checksum = calculateChecksum(&hdr, sizeof(hdr));

	int t;

	if ((t = sendto(sock_fd, &hdr, sizeof(hdr), 0, (const struct sockaddr *)reflector, sizeof(*reflector))) == -1)
	{
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &reflector->sin_addr, &ip, INET_ADDRSTRLEN);

		printf("something wrong for ip %s: %d\n", ip, t);
		return 1;
	}

	return 0;
}

void *icmp_receiver_loop(int sock_fd)
{
	while (1)
	{
		char * buff = malloc(100);
		struct icmp_timestamp_hdr * hdr;
		struct sockaddr_in remote_addr;
		socklen_t addr_len = sizeof(remote_addr);
		struct __kernel_timespec rxTimestamp;
		int recv = recvfrom(sock_fd, buff, 100, 0, (struct sockaddr *)&remote_addr, &addr_len);

		if (recv < 0)
			continue;

		int len = (*buff & 0x0F) * 4;

		if (len + sizeof(struct icmp_timestamp_hdr) > recv)
		{
			printf("Not enough data, skipping\n");
			continue;
		}

		hdr = (struct icmp_timestamp_hdr *) (buff + len);

		if (hdr->type != ICMP_TIMESTAMPREPLY)
		{
			//printf("icmp: get outta here: %d\n", hdr->type);
			continue;
		}

		//if (get_rx_timestamp(sock_fd, &rxTimestamp) == -1)
		//{
		//	printf("couldn't get rx ts, fallback to current time\n");
			rxTimestamp = get_time();
		//}

		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(remote_addr.sin_addr), ip, INET_ADDRSTRLEN);

		unsigned long now_ts = (rxTimestamp.tv_sec % 86400 * 1000) + (rxTimestamp.tv_nsec / 1000000);
		unsigned long rtt = now_ts - ntohl(hdr->originateTime);
		unsigned long uplink_time = ntohl(hdr->receiveTime) - ntohl(hdr->originateTime);
		unsigned long downlink_time = now_ts - ntohl(hdr->transmitTime);

		printf("Type: %4s  |  Reflector IP: %15s  |  Seq: %5d  |  Current time: %8ld  |  Originate: %8ld  |  Received time: %8ld  |  Transmit time: %8ld  |  RTT: %5ld  |  UL time: %5ld  |  DL time: %5ld\n", 
		"ICMP", ip, ntohs(hdr->sequence), now_ts, (unsigned long) ntohl(hdr->originateTime), (unsigned long) ntohl(hdr->receiveTime), (unsigned long) ntohl(hdr->transmitTime), rtt, uplink_time, downlink_time);
		free(buff);
	}
}