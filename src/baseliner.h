#ifndef BASELINER_H
#define BASELINER_H

#include <netinet/in.h>
#include <stdint.h>

#include "uthash.h"

typedef struct owd_data_s {
    char reflector[INET_ADDRSTRLEN];
    float down_ewma;
    float up_ewma;
    uint32_t last_receive_time_s;
    UT_hash_handle hh;
} owd_data_t;

typedef struct time_data_s {
    char reflector[INET_ADDRSTRLEN];
    uint32_t originate_timestamp;
    uint32_t receive_timestamp;
    uint32_t transmit_timestamp;
    uint32_t rtt;
    uint32_t downlink_time;
    uint32_t uplink_time;
    uint32_t last_receive_time_s;
} time_data_t;

void * baseliner_loop();

#endif // BASELINER_H