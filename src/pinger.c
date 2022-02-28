#include <arpa/inet.h>
#include <math.h>
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

	pthread_rwlock_rdlock(&reflectors_lock);
	int amount_of_reflectors = HASH_COUNT(reflectors);
	pthread_rwlock_unlock(&reflectors_lock);

	double sec;
	double nsec = modf(tick_duration / amount_of_reflectors, &sec) * 1e9;
	wait_time.tv_sec = sec;
	wait_time.tv_nsec = nsec;

	printf("sec: %f\n", sec);
	printf("nsec: %f\n", nsec);

	int seq = 0;

	while (1)
	{
		pthread_rwlock_rdlock(&reflectors_lock);
		reflector_t * reflector;

		for (reflector = reflectors; reflector != NULL; reflector = reflector->hh.next) {
        	char str[INET_ADDRSTRLEN];

			inet_ntop(AF_INET, &(reflector->addr->sin_addr), str, INET_ADDRSTRLEN);
			icmp_ping_send(sock_fd, reflector->addr, htons(seq));

			nanosleep(&wait_time, NULL);
    	}

		pthread_rwlock_unlock(&reflectors_lock);

		seq++;
	}

	exit(0);
}