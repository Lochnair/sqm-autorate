#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <linux/time_types.h>

#include "baseliner.h"
#include "pinger_icmp.h"

#define __USE_UNIX98
#include <pthread.h>
#include "pthread_queue.h"
#include "utils.h"

#include "globals.h"

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
		inet_ntop(AF_INET, &reflector->sin_addr, (char *) &ip, INET_ADDRSTRLEN);

		log_error("Something went wrong when sending to IP: %s: %d", ip, t);
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
		struct timespec current_time;
		int recv = recvfrom(sock_fd, buff, 100, 0, (struct sockaddr *)&remote_addr, &addr_len);

		if (recv < 0)
			continue;

		int len = (*buff & 0x0F) * 4;

		if (len + sizeof(struct icmp_timestamp_hdr) > recv)
		{
			log_warn("Not enough data, skipping");
			continue;
		}

		hdr = (struct icmp_timestamp_hdr *) (buff + len);

		if (hdr->type != ICMP_TIMESTAMPREPLY)
		{
			log_trace("Wrong ICMP type: %d", hdr->type);
			continue;
		}

		current_time = get_time();
		unsigned long time_since_midnight_ms = (current_time.tv_sec % 86400 * 1000) + (current_time.tv_nsec / 1000000);

		time_data_t time_data;
		inet_ntop(AF_INET, &(remote_addr.sin_addr), (char *) &time_data.reflector, INET_ADDRSTRLEN);

		pthread_rwlock_rdlock(&reflector_peers_lock);
		reflector_t * reflector = (reflector_t *) ht_search(reflector_peers, time_data.reflector);
		pthread_rwlock_unlock(&reflector_peers_lock);

		if (!reflector) // if reflector not in hash table, ignore it
		{
			continue;
		}

		time_data.originate_timestamp = ntohl(hdr->originateTime);
		time_data.receive_timestamp = ntohl(hdr->receiveTime);
		time_data.transmit_timestamp = ntohl(hdr->transmitTime);
		time_data.rtt = time_since_midnight_ms - time_data.originate_timestamp;
		time_data.downlink_time = time_since_midnight_ms - time_data.transmit_timestamp;
		time_data.uplink_time = time_data.receive_timestamp - time_data.originate_timestamp;
		time_data.last_receive_time_s = current_time.tv_sec + current_time.tv_nsec / 1e9;

		log_debug("Type: %4s  |  Reflector IP: %15s  |  Seq: %5d  |  Current time: %8ld  |  Originate: %8ld  |  Received time: %8ld  |  Transmit time: %8ld  |  RTT: %5ld  |  UL time: %5ld  |  DL time: %5ld", 
		"ICMP", time_data.reflector, ntohs(hdr->sequence), time_since_midnight_ms, (unsigned long) time_data.originate_timestamp, (unsigned long) time_data.receive_timestamp, (unsigned long) time_data.transmit_timestamp, (unsigned long) time_data.rtt, (unsigned long) time_data.uplink_time, (unsigned long) time_data.downlink_time);
		free(buff);

		pthread_queue_sendmsg(baseliner_queue, &time_data, 100);
	}
}