#ifndef GLOBALS_H
#define GLOBALS_H

#define _GNU_SOURCE
#include <pthread.h>
#include "pthread_queue.h"

#include "baseliner.h"
#include "reflectors.h"

extern pthread_queue_t * baseliner_queue;

extern reflector_t * reflectors;
extern pthread_rwlock_t reflectors_lock;

extern pthread_queue_t * reselector_channel;

extern owd_data_t * owd_baseline, * owd_recent;
extern pthread_rwlock_t owd_lock;

extern int sock_fd;

extern const float tick_duration;

#endif // GLOBALS_H