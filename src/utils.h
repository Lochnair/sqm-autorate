#ifndef UTILS_H
#define UTILS_H

#include <linux/time_types.h>

#include "reflectors.h"

unsigned short calculateChecksum(void *b, int len);

double ewma_factor(float tick, float dur);

int get_rx_timestamp(int sock_fd, struct __kernel_timespec * rx_timestamp);

struct timespec get_time();

unsigned long get_time_since_midnight_ms();

void hexDump (
    const char * desc,
    const void * addr,
    const int len,
    int perLine
);

int load_initial_peers();
int load_reflector_list(char * path);

int nsleep(long sec, long nsec);

void shuffle_table(int *array, int n);

#endif // UTILS_H