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
#include "ratecontroller.h"
#include "reflectors.h"
#include "utils.h"

int sock_fd;

pthread_queue_t * baseliner_queue;

owd_data_t * owd_baseline, * owd_recent;
pthread_rwlock_t owd_lock;

const float tick_duration = 0.5;

reflector_t * reflector_peers = NULL;
pthread_rwlock_t reflector_peers_lock;

reflector_t * reflector_pool = NULL;
pthread_rwlock_t reflector_pool_lock;

pthread_queue_t * reselector_channel;

char * dl_if, * ul_if;

char * rx_bytes_path, * tx_bytes_path;

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

int test_if_file_exists(char * path, int retries, int retry_time)
{
	FILE * test_file = fopen(path, "r");

	if (test_file == NULL)
	{
		/*
		 * Let's wait and retry a few times before failing hard. These files typically
         * take some time to be generated following a reboot.
		 */
		struct timespec wait_time = {.tv_sec = retry_time};

		for (int i = 0; i < retries; i++)
		{
			printf("Stats file %s not yet available. Will retry again in %ld seconds. (Attempt %d of %d)\n", path, wait_time.tv_sec, i, retries);
			nanosleep(&wait_time, NULL);
			test_file = fopen(path, "r");
			if (test_file)
			{
				break;
			}
		}

		if (test_file == NULL)
		{
			printf("Could not open stats file: %s\n", path);
			exit(1);
		}
	}

	fclose(test_file);
	return 1;
}

void set_statistics_paths()
{
	printf("dl_if: %s\n", dl_if);
	printf("ul_if: %s\n", ul_if);

	// Verify these are correct using "cat /sys/class/..."
	if (strstr(dl_if, "ifb") == dl_if || strstr(dl_if, "veth") == dl_if)
	{
		// length of constants + interface length and space for null terminator
		rx_bytes_path = calloc(1, 35 + strlen(dl_if) + 1);
		strcat(rx_bytes_path, "/sys/class/net/");
		strcat(rx_bytes_path, dl_if);
		strcat(rx_bytes_path, "/statistics/tx_bytes");
	}
	else if(strncmp(dl_if, "br-lan", 6) == 0)
	{
		// length of constants + interface length and space for null terminator
		rx_bytes_path = calloc(1, 35 + strlen(ul_if) + 1);
		strcat(rx_bytes_path, "/sys/class/net/");
		strcat(rx_bytes_path, ul_if);
		strcat(rx_bytes_path, "/statistics/rx_bytes");
	}
	else
	{
		// length of constants + interface length and space for null terminator
		rx_bytes_path = calloc(1, 35 + strlen(dl_if) + 1);
		strcat(rx_bytes_path, "/sys/class/net/");
		strcat(rx_bytes_path, dl_if);
		strcat(rx_bytes_path, "/statistics/rx_bytes");
	}

	if (strstr(ul_if, "ifb") == ul_if || strstr(ul_if, "veth") == ul_if)
	{
		// length of constants + interface length and space for null terminator
		tx_bytes_path = calloc(1, 35 + strlen(ul_if) + 1);
		strcat(tx_bytes_path, "/sys/class/net/");
		strcat(tx_bytes_path, ul_if);
		strcat(tx_bytes_path, "/statistics/rx_bytes");
	}
	else
	{
		// length of constants + interface length and space for null terminator
		tx_bytes_path = calloc(1, 35 + strlen(ul_if) + 1);
		strcat(tx_bytes_path, "/sys/class/net/");
		strcat(tx_bytes_path, ul_if);
		strcat(tx_bytes_path, "/statistics/tx_bytes");
	}

	printf("rx path: %s\n", rx_bytes_path);
	printf("tx path: %s\n", tx_bytes_path);

	test_if_file_exists(rx_bytes_path, 12, 5);
	printf("Download device stats file found! Continuing...\n");
	test_if_file_exists(tx_bytes_path, 12, 5);
	printf("Upload device stats file found! Continuing...\n");
}

#define CREATE_THREAD(name, function) ({\
	int name_create_result; \
	pthread_t name_thread; \
	if ((name_create_result = pthread_create(&name_thread, NULL, function, NULL)) != 0) \
	{\
		printf("failed to create %s thread: %d\n", name, t);\
		exit(1);\
	}\
	pthread_setname_np(name_thread, name);\
})

#define JOIN_THREAD(name) ({\
	int name_status_ret; \
	void *name_status; \
	if ((name_status_ret = pthread_join(name_thread, &name_status)) != 0) \
	{\
		printf("Error in %s thread join: %d\n", name, name_status_ret);\
	}\
})

int main()
{
	time_t now;

	// Initialize random number generator
	srand((unsigned) time(&now));

	if ((pthread_rwlock_init(&owd_lock, NULL)) != 0)
	{
		printf("can't create rwlock");
		return 1;
	}

	if ((pthread_rwlock_init(&reflector_peers_lock, NULL)) != 0)
	{
		printf("can't create rwlock");
		return 1;
	}

	if ((pthread_rwlock_init(&reflector_pool_lock, NULL)) != 0)
	{
		printf("can't create rwlock");
		return 1;
	}

	load_initial_peers();
	load_reflector_list("./reflectors-icmp.csv");

	dl_if = "lanifb";
	ul_if = "lan";

	set_statistics_paths();

	sock_fd = get_icmp_socket();

	pthread_queue_create(&baseliner_queue, NULL, 32, sizeof(time_data_t));
	pthread_queue_create(&reselector_channel, NULL, 10, 1);

	/*
	 * Set initial TC values to minimum
     * so there should be no initial bufferbloat to
     * fool the baseliner
	 */
	update_cake_bandwidth(dl_if, 5000);
	update_cake_bandwidth(ul_if, 2000);
	nsleep(0, 5e8);

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
	void *reselector_status;
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
