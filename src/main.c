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
#include "log.h"
#include "pinger.h"
#include "ratecontroller.h"
#include "reflectors.h"
#include "settings.h"
#include "utils.h"

int sock_fd;

pthread_queue_t * baseliner_queue;

pthread_rwlock_t owd_lock;
pthread_rwlock_t reflector_peers_lock;
pthread_rwlock_t reflector_pool_lock;

pthread_queue_t * reselector_channel;

settings_t settings;

int get_icmp_socket()
{
    int icmp_sock_fd;

    if ((icmp_sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
	{
		log_fatal("Couldn't get a ICMP socket: %d", icmp_sock_fd);
		return -1;
	}

	int ts_enable = 0;

    if (setsockopt(icmp_sock_fd, SOL_SOCKET, SO_TIMESTAMPNS_NEW, &ts_enable, sizeof(ts_enable)) == -1)
	{
		log_error("Couldn't set timestamp option on ICMP socket");
	}

    return icmp_sock_fd;
}

int get_udp_socket()
{
    int udp_sock_fd;

    if ((udp_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		log_fatal("Couldn't get a UDP socket: %d", udp_sock_fd);
		return -1;
	}

	int ts_enable = 0;

    if (setsockopt(udp_sock_fd, SOL_SOCKET, SO_TIMESTAMPNS_NEW, &ts_enable, sizeof(ts_enable)) == -1)
	{
		log_error("Couldn't set timestamp option on UDP socket");
	}

    return udp_sock_fd;
}

#define CREATE_RWLOCK(name) \
	int name##_create_result; \
	if ((name##_create_result = pthread_rwlock_init(&name, NULL)) != 0) \
	{\
		log_fatal("Couldn't create rwlock %s -> %d", #name, name##_create_result); \
		return 1; \
	}

#define CREATE_THREAD(name, function)\
	int name##_create_result; \
	pthread_t name##_thread; \
	if ((name##_create_result = pthread_create(&name##_thread, NULL, function, NULL)) != 0) \
	{\
		log_fatal("Failed to create %s thread: %d", #name, name##_create_result);\
		exit(1);\
	}\
	pthread_setname_np(name##_thread, #name);

#define JOIN_THREAD(name)\
	int name##_status_ret; \
	void *name##_status; \
	if ((name##_status_ret = pthread_join(name##_thread, &name##_status)) != 0) \
	{\
		log_fatal("Error in %s thread join: %d", #name, name##_status_ret);\
	}

void log_locker(bool lock, void *udata)
{
	pthread_mutex_t * log_lock = (pthread_mutex_t *) udata;

	if (lock)
		pthread_mutex_lock(log_lock);
	else
		pthread_mutex_unlock(log_lock);
}

int main()
{
	time_t now;

	// Initialize random number generator
	srand((unsigned) time(&now));

	pthread_mutex_t log_lock;

	if (pthread_mutex_init(&log_lock, NULL) != 0)
	{
		printf("Couldn't initialize mutex for logging.\n");
		return 1;
	}

	// Initialize logger
	log_set_lock(log_locker, &log_lock);
	log_set_level(LOG_DEBUG);

	log_info("Starting sqm-autorate v0.0.1");

	CREATE_RWLOCK(owd_lock);
	CREATE_RWLOCK(reflector_peers_lock);
	CREATE_RWLOCK(reflector_pool_lock);

	if (load_settings(&settings) != 0)
		return 1;

	if ((sock_fd = get_icmp_socket()) == -1)
		return 1;

	load_initial_peers();
	load_reflector_list("./reflectors-icmp.csv");

	pthread_queue_create(&baseliner_queue, NULL, 32, sizeof(time_data_t));
	pthread_queue_create(&reselector_channel, NULL, 10, 1);

	/*
	 * Set initial TC values to minimum
     * so there should be no initial bufferbloat to
     * fool the baseliner
	 */
	update_cake_bandwidth(settings.dl_if, 5000);
	update_cake_bandwidth(settings.ul_if, 2000);
	nsleep(0, 5e8);

	CREATE_THREAD(baseliner, baseliner_loop);
	CREATE_THREAD(receiver, receiver_loop);
	CREATE_THREAD(reselector, reflector_peer_selector);
	CREATE_THREAD(sender, sender_loop);

	// sleep 10 seconds before we start adjusting speeds
	nsleep(10, 0);
	CREATE_THREAD(ratecontroller, ratecontroller_loop);

	JOIN_THREAD(baseliner);
	JOIN_THREAD(ratecontroller);
	JOIN_THREAD(receiver);
	JOIN_THREAD(reselector);
	JOIN_THREAD(sender);

	return 0;
}
