#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>

#include "pinger_icmp.h"

extern struct sockaddr_in *reflectors;
extern int reflectors_len;
extern int sock_fd;

void * receiver_loop()
{
	icmp_receiver_loop(sock_fd);
	return 0;
}

void * sender_loop()
{
	struct timespec wait_time;

	wait_time.tv_sec = 1;
	wait_time.tv_nsec = 0;

	int seq = 0;

	printf("reflectors: %d\n", reflectors_len);

	while (1)
	{
		for (int i = 0; i < reflectors_len; i++)
		{
			char str[INET_ADDRSTRLEN];

			inet_ntop(AF_INET, &(reflectors[i].sin_addr), str, INET_ADDRSTRLEN);
			icmp_ping_send(sock_fd, &reflectors[i], htons(seq));
		}

		seq++;
		nanosleep(&wait_time, NULL);
	}

	exit(0);
}