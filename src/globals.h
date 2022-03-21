#ifndef GLOBALS_H
#define GLOBALS_H

#define _GNU_SOURCE
#include <pthread.h>
#include "pthread_queue.h"

#include "baseliner.h"
#include "ratecontroller.h"
#include "reflectors.h"
#include "settings.h"

extern pthread_queue_t * baseliner_queue;

extern reflector_t * reflector_peers;
extern pthread_rwlock_t reflector_peers_lock;

extern reflector_t * reflector_pool;
extern pthread_rwlock_t reflector_pool_lock;

extern pthread_queue_t * reselector_channel;

extern owd_data_t * owd_baseline, * owd_recent;
extern pthread_rwlock_t owd_lock;

extern settings_t settings;

extern int sock_fd;

#endif // GLOBALS_H