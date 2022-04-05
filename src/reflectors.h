#ifndef REFLECTORS_H
#define REFLECTORS_H

#include <netinet/in.h>
#include "uthash.h"

typedef struct reflector_s {
    struct sockaddr_in * addr;
    char ip[INET_ADDRSTRLEN];
    int rtt; // used in reselection to pick reflectors
} reflector_t;

void load_initial_peers();
void load_reflector_list(char * path);

void * reflector_peer_selector();

#endif // REFLECTORS_H