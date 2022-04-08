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

	int seq = 0;

	while (1)
	{

		pthread_rwlock_rdlock(&reflector_peers_lock);
		int amount_of_reflectors = reflector_peers->count, size = reflector_peers->size;;
		ht_item ** items = (ht_item **) calloc((size_t)size, sizeof(ht_item*));
		memcpy(items, reflector_peers->items, size * sizeof(ht_item*));
		pthread_rwlock_unlock(&reflector_peers_lock);
		reflector_t * reflector = NULL;

		double sec;
		double nsec = modf(settings.tick_duration / amount_of_reflectors, &sec) * 1e9;

		wait_time.tv_sec = sec;
		wait_time.tv_nsec = nsec;

		for (int i = 0; i < size; i++)
        {
            ht_item * item = items[i];

            if (item == NULL || item == &HT_DELETED_ITEM)
                continue;

			reflector = (reflector_t *) item->value;
			icmp_ping_send(sock_fd, reflector->addr, htons(seq));

			free(item);
			nanosleep(&wait_time, NULL);
    	}

		free(items);

		seq++;
	}

	exit(0);
}