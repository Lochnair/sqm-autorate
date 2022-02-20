#ifndef UTILS_H
#define UTILS_H

#include <linux/time_types.h>

unsigned short calculateChecksum(void *b, int len);

int get_rx_timestamp(int sock_fd, struct __kernel_timespec * rx_timestamp);

struct __kernel_timespec get_time();

unsigned long get_time_since_midnight_ms();

void hexDump (
    const char * desc,
    const void * addr,
    const int len,
    int perLine
);

#endif // UTILS_H