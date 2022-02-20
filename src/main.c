#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "pinger.h"

#define _GNU_SOURCE

int sock_fd;

static const char *const ips[] = {"65.21.108.153", "5.161.66.148", "185.243.217.26", "185.175.56.188", "176.126.70.119", "216.128.149.82", "108.61.220.16"};
struct sockaddr_in *reflectors;
int reflectors_len = -1;

int get_icmp_socket()
{
    int icmp_sock_fd;

    if ((icmp_sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
	{
		printf("no icmp socket for you\n");
		return -1;
	}

	int ts_enable = 0;

    if (setsockopt(icmp_sock_fd, SOL_SOCKET, SO_TIMESTAMPNS_NEW, &ts_enable, sizeof(ts_enable)) == -1)
	{
		printf("couldn't set ts option on icmp socket\n");
		return -1;
	}

    return icmp_sock_fd;
}

int get_udp_socket()
{
    int udp_sock_fd;

    if ((udp_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		printf("no udp socket for you\n");
		return -1;
	}

	int ts_enable = 0;

    if (setsockopt(udp_sock_fd, SOL_SOCKET, SO_TIMESTAMPNS_NEW, &ts_enable, sizeof(ts_enable)) == -1)
	{
		printf("couldn't set ts option on udp socket\n");
		return -1;
	}

    return udp_sock_fd;
}

int main()
{
	sock_fd = get_icmp_socket();

	reflectors_len = sizeof(ips) / sizeof(ips[0]);
	reflectors = malloc(sizeof(struct sockaddr_in) * reflectors_len);

	for (int i = 0; i < reflectors_len; i++)
	{
		inet_pton(AF_INET, ips[i], &reflectors[i].sin_addr);
		reflectors[i].sin_port = htons(62222);
	}

	pthread_t receiver_thread;
	pthread_t sender_thread;

	int t;
	if ((t = pthread_create(&receiver_thread, NULL, receiver_loop, NULL)) != 0)
	{
		printf("failed to create icmp receiver thread: %d\n", t);
	}

	if ((t = pthread_create(&sender_thread, NULL, sender_loop, NULL)) != 0)
	{
		printf("failed to create sender thread: %d\n", t);
	}

	pthread_setname_np(receiver_thread, "receiver");
	pthread_setname_np(sender_thread, "sender");

	void *receiver_status;
	void *sender_status;

	if ((t = pthread_join(receiver_thread, &receiver_status)) != 0)
	{
		printf("Error in icmp receiver thread join: %d\n", t);
	}

	if ((t = pthread_join(sender_thread, &sender_status)) != 0)
	{
		printf("Error in sender thread join: %d\n", t);
	}

	/*struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 100000;
	//setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);*/

	return 0;
}
