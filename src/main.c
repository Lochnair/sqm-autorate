#define _GNU_SOURCE
#include <features.h>

#include <arpa/inet.h>
#include <asm/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <pthread.h>
#include "pthread_queue.h"

#include "baseliner.h"
#include "pinger.h"
#include "reflectors.h"
#include "utils.h"

int sock_fd;

pthread_queue_t * baseliner_queue;

owd_data_t * owd_baseline, * owd_recent;
pthread_rwlock_t owd_lock;

const float tick_duration = 0.5;

//static const char *const ips[] = {"65.21.108.153", "5.161.66.148", "185.243.217.26", "185.175.56.188", "176.126.70.119", "216.128.149.82", "108.61.220.16"};
static const char *const ips[] = {"9.9.9.9", "9.9.9.10", "172.18.254.1", "208.67.222.220", "208.67.222.222"};
//struct sockaddr_in *reflectors;

reflector_t * reflectors = NULL;
pthread_rwlock_t reflectors_lock;

pthread_queue_t * reselector_channel;

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

int init_reflectors()
{
	pthread_rwlock_wrlock(&reflectors_lock);
	
	int reflectors_len = sizeof(ips) / sizeof(ips[0]);

	for (int i = 0; i < reflectors_len; i++)
	{
		reflector_t * new_reflector = calloc(1, sizeof(reflector_t));
		new_reflector->addr = calloc(1, sizeof(struct sockaddr_in));

		strcpy((char *) &new_reflector->ip, ips[i]);
		inet_pton(AF_INET, (char *) &new_reflector->ip, &new_reflector->addr->sin_addr);
		new_reflector->addr->sin_port = htons(62222);
		HASH_ADD_STR(reflectors, ip, new_reflector);
	}

	pthread_rwlock_unlock(&reflectors_lock);

	return 0;
}

int main()
{
	if ((pthread_rwlock_init(&owd_lock, NULL)) != 0)
	{
		printf("can't create rwlock");
		return 1;
	}

	if ((pthread_rwlock_init(&reflectors_lock, NULL)) != 0)
	{
		printf("can't create rwlock");
		return 1;
	}

	load_reflector_list("./reflectors-icmp.csv", &reflectors);

	sock_fd = get_icmp_socket();

	pthread_queue_create(&baseliner_queue, NULL, 32, sizeof(time_data_t));
	pthread_queue_create(&reselector_channel, NULL, 10, 1);

	pthread_t baseliner_thread;
	pthread_t receiver_thread;
	pthread_t reselector_thread;
	pthread_t sender_thread;

	int t;
	if ((t = pthread_create(&baseliner_thread, NULL, baseliner_loop, NULL)) != 0)
	{
		printf("failed to create baseliner thread: %d\n", t);
	}

	if ((t = pthread_create(&receiver_thread, NULL, receiver_loop, NULL)) != 0)
	{
		printf("failed to create icmp receiver thread: %d\n", t);
	}

	if ((t = pthread_create(&reselector_thread, NULL, reflector_peer_selector, NULL)) != 0)
	{
		printf("failed to create reselector thread: %d\n", t);
	}

	if ((t = pthread_create(&sender_thread, NULL, sender_loop, NULL)) != 0)
	{
		printf("failed to create sender thread: %d\n", t);
	}

	pthread_setname_np(baseliner_thread, "baseliner");
	pthread_setname_np(receiver_thread, "receiver");
	pthread_setname_np(reselector_thread, "reselector");
	pthread_setname_np(sender_thread, "sender");

	void *baseliner_status;
	void *receiver_status;
	void *sender_status;

	if ((t = pthread_join(baseliner_thread, &baseliner_status)) != 0)
	{
		printf("Error in baseliner thread join: %d\n", t);
	}

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
