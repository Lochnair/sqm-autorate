#ifndef GLOBALS_H
#define GLOBALS_H

#define __USE_UNIX98 1
#include <pthread.h>
#include "pthread_ext_common.h"
#include "pthread_queue.h"

#include "baseliner.h"
#include "hash_table.h"
#include "log.h"
#include "ratecontroller.h"
#include "reflectors.h"
#include "settings.h"

extern pthread_queue_t * baseliner_queue;

extern hash_table * reflector_peers;
extern pthread_rwlock_t reflector_peers_lock;

extern hash_table * reflector_pool;
extern pthread_rwlock_t reflector_pool_lock;

extern pthread_queue_t * reselector_channel;

extern hash_table * owd_data;
extern pthread_rwlock_t owd_lock;

extern settings_t settings;

extern int sock_fd;

extern ht_item HT_DELETED_ITEM;

#endif // GLOBALS_H