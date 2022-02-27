#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>

#include "pinger_icmp.h"

#include "globals.h"

void * receiver_loop()
{
	icmp_receiver_loop(sock_fd);
	return 0;
}

void * sender_loop()
{
	struct timespec wait_time;

	wait_time.tv_sec = 0;
	wait_time.tv_nsec = 100000000;

	int seq = 0;

	while (1)
	{
		reflector_t * reflector;

		for (reflector = reflectors; reflector != NULL; reflector = reflector->hh.next) {
        	char str[INET_ADDRSTRLEN];

			inet_ntop(AF_INET, &(reflector->addr->sin_addr), str, INET_ADDRSTRLEN);
			icmp_ping_send(sock_fd, reflector->addr, htons(seq));

			nanosleep(&wait_time, NULL);
    	}

		seq++;
	}

	exit(0);
}