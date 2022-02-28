#ifndef REFLECTORS_H
#define REFLECTORS_H

#include <netinet/in.h>
#include "uthash.h"

typedef struct reflector_s {
    struct sockaddr_in * addr;

    char ip[INET_ADDRSTRLEN];

    UT_hash_handle hh;
} reflector_t;

#endif // REFLECTORS_H