#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include "pinger_udp.h"
#include "log.h"
#include "utils.h"

int udp_ping_send(int sock_fd, struct sockaddr_in *reflector, int seq)
{
	struct udp_timestamp_hdr hdr;

	memset(&hdr, 0, sizeof(hdr));

	struct timespec now = get_time();

	hdr.type = ICMP_TIMESTAMP;
	hdr.identifier = htons(0xFEED);
	hdr.sequence = seq;
	hdr.originateTime = htonl(now.tv_sec);
	hdr.originateTimeNs = htonl(now.tv_nsec);

	hdr.checksum = calculateChecksum(&hdr, sizeof(hdr));

	int t;

	if ((t = sendto(sock_fd, &hdr, sizeof(hdr), 0, (const struct sockaddr *)reflector, sizeof(*reflector))) == -1)
	{
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &reflector->sin_addr, (char *) &ip, INET_ADDRSTRLEN);

		log_error("Something went wrong when sending to IP: %s: %d", ip, t);
		return 1;
	}

	return 0;
}

void *udp_receiver_loop(int sock_fd)
{
	while (1)
	{
		struct udp_timestamp_hdr hdr;
		struct sockaddr_in remote_addr;
		socklen_t addr_len = sizeof(remote_addr);
		struct __kernel_timespec rxTimestamp;
		int recv = recvfrom(sock_fd, &hdr, sizeof(hdr), 0, (struct sockaddr *)&remote_addr, &addr_len);

		if (recv == -1)
			continue;

		if (recv != 32)
		{
			log_debug("Wrong size of packet: %d", recv);
			continue;
		}

		if (hdr.type != ICMP_TIMESTAMPREPLY)
		{
			log_trace("Wrong ICMP type: %d", hdr.type);
			continue;
		}

		if (get_rx_timestamp(sock_fd, &rxTimestamp) == -1)
		{
			log_warn("Couldn't get receive timestamp, fallback to current time");
			
		}

		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(remote_addr.sin_addr), ip, INET_ADDRSTRLEN);

		unsigned long originate_ts = (ntohl(hdr.originateTime) % 86400 * 1000) + (ntohl(hdr.originateTimeNs) / 1000000);
		unsigned long received_ts = (ntohl(hdr.receiveTime) % 86400 * 1000) + (ntohl(hdr.receiveTimeNs) / 1000000);
		unsigned long transmit_ts = (ntohl(hdr.transmitTime) % 86400 * 1000) + (ntohl(hdr.transmitTimeNs) / 1000000);
		unsigned long now_ts = (rxTimestamp.tv_sec % 86400 * 1000) + (rxTimestamp.tv_nsec / 1000000);
		unsigned long rtt = now_ts - originate_ts;
		unsigned long uplink_time = received_ts - originate_ts;
		unsigned long downlink_time = now_ts - transmit_ts;

		log_debug("Type: %4s  |  Reflector IP: %15s  |  Seq: %5d  |  Current time: %8ld  |  Originate: %8ld  |  Received time: %8ld  |  Transmit time: %8ld  |  RTT: %5ld  |  UL time: %5ld  |  DL time: %5ld", 
		"UDP", ip, ntohs(hdr.sequence), now_ts, originate_ts, received_ts, transmit_ts, rtt, uplink_time, downlink_time);
	}
}