#ifndef REFLECTORS_H
#define REFLECTORS_H

#include <netinet/in.h>
#include "uthash.h"

typedef struct reflector_s {
    struct sockaddr_in * addr;
    char ip[INET_ADDRSTRLEN];
    int rtt; // used in reselection to pick reflectors
    UT_hash_handle hh;
} reflector_t;

void * reflector_peer_selector();

#endif // REFLECTORS_H