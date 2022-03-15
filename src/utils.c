#include <arpa/inet.h>
#include <asm/socket.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#define __USE_UNIX98
#include <pthread.h>
#include "pthread_queue.h"

#include "globals.h"
#include "reflectors.h"
#include "utils.h"

unsigned short calculateChecksum(void *b, int len)
{
	unsigned short *buf = b;
	unsigned int sum = 0;
	unsigned short result;

	for (sum = 0; len > 1; len -= 2)
		sum += *buf++;
	if (len == 1)
		sum += *(unsigned char *)buf;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}

double ewma_factor(float tick, float dur)
{
    return exp(logf(0.5) / (dur / tick));
}

struct timespec get_time()
{
	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time);
	return time;
}

unsigned long get_time_since_midnight_ms()
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);

    return (time.tv_sec % 86400 * 1000) + (time.tv_nsec / 1000000);
}

int get_rx_timestamp(int sock_fd, struct __kernel_timespec * rx_timestamp)
{
	struct msghdr msg;
	struct iovec iov;
	char buffer[2048];
	char control[1024];

	iov.iov_base = buffer;
	iov.iov_len = 2048;

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = control;
	msg.msg_controllen = 1024;

	int got = recvmsg(sock_fd, &msg, 0);

	if (!got)
		return -1;

	for (struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg))
	{
		if (cmsg->cmsg_level != SOL_SOCKET)
			continue;

		switch (cmsg->cmsg_type)
		{
			case SO_TIMESTAMPNS_NEW:
				memcpy(rx_timestamp, CMSG_DATA(cmsg), sizeof(struct __kernel_timespec));
				return 0;
		}
	}

	return -1;
}

void hexDump (
    const char * desc,
    const void * addr,
    const int len,
    int perLine
) {
    // Silently ignore silly per-line values.

    if (perLine < 4 || perLine > 64) perLine = 16;

    int i;
    unsigned char buff[perLine+1];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.

    if (desc != NULL) printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of perLine means new or first line (with line offset).

        if ((i % perLine) == 0) {
            // Only print previous-line ASCII buffer for lines beyond first.

            if (i != 0) printf ("  %s\n", buff);

            // Output the offset of current line.

            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.

        printf (" %02x", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % perLine] = '.';
        else
            buff[i % perLine] = pc[i];
        buff[(i % perLine) + 1] = '\0';
    }

    // Pad out last line if not exactly perLine characters.

    while ((i % perLine) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}

int add_reflector(char * ip, reflector_t ** out, pthread_rwlock_t * out_lock)
{
    reflector_t * new_reflector = calloc(1, sizeof(reflector_t));
    new_reflector->addr = calloc(1, sizeof(struct sockaddr_in));

    strcpy((char *) &new_reflector->ip, ip);
    inet_pton(AF_INET, (char *) &new_reflector->ip, &new_reflector->addr->sin_addr);
    new_reflector->addr->sin_port = htons(62222);

    pthread_rwlock_wrlock(out_lock);
    HASH_ADD_STR(*out, ip, new_reflector);
    pthread_rwlock_unlock(out_lock);

    return 0;
}

int load_initial_peers()
{
    char * initial_reflectors[] = {"9.9.9.9", "8.238.120.14", "74.82.42.42", "194.242.2.2", "208.67.222.222", "94.140.14.14"};

    for (int i = 0; i < sizeof(initial_reflectors) / sizeof(initial_reflectors[0]); i++) {
        add_reflector(initial_reflectors[i], &reflector_peers, &reflector_peers_lock);
    }

    return 0;
}

int load_reflector_list(char * path)
{
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(path, "r");
    
    if (fp == NULL)
    {
        printf("Could not open reflector file: %s\n", path);
        return -1;
    }

    int i = 0;

    while ((read = getline(&line, &len, fp)) != -1) {
        // skip first line
        if (i == 0) {
            i++;
            continue;
        }

        char * ip = strtok(line, ",");
        add_reflector(ip, &reflector_pool, &reflector_pool_lock);
        i++;
    }

    fclose(fp);
    if (line)
        free(line);

    pthread_rwlock_rdlock(&reflector_pool_lock);
    int reflector_count = HASH_COUNT(reflector_pool);
    pthread_rwlock_unlock(&reflector_pool_lock);

    printf("Reflector pool size: %d\n", reflector_count);

    return 0;
}

int nsleep(long sec, long nsec)
{
    struct timespec sleep_time = {.tv_sec = sec, .tv_nsec = nsec};
    return nanosleep(sleep_time, NULL);
}