#define _GNU_SOURCE
#include <arpa/inet.h>
#include <math.h>
#include <stdio.h>

#include "globals.h"
#include "utils.h"

#define rand_in_range(min, max) rand() % (max + 1 - min) + min

hash_table * reflector_peers = NULL;
hash_table * reflector_pool = NULL;

int rtt_sort(const void * a, const void * b)
{
    if (a == NULL || b == NULL)
        return 0;

    ht_item * item_a = *(ht_item **) a;
    ht_item * item_b = *(ht_item **) b;

    if (item_a == NULL && item_b == NULL)
        return 0;

    log_debug("deref ptr a,b: %p,%p", item_a, item_b);

    if (item_a == NULL)
        return 1;

    if (item_b == NULL)
        return -1;

    if (item_a == &HT_DELETED_ITEM || item_b == &HT_DELETED_ITEM)
        return 0;

    reflector_t * refl_a = (reflector_t *) item_a->value;
    reflector_t * refl_b = (reflector_t *) item_b->value;

    log_debug("rtt a,b: %d,%d", refl_a->rtt, refl_b->rtt);

    return refl_a->rtt - refl_b->rtt;
}

void add_reflector(char * ip, hash_table * ht)
{
    reflector_t * new_reflector = calloc(1, sizeof(reflector_t));
    strncpy(new_reflector->ip, ip, INET_ADDRSTRLEN);

    new_reflector->addr = calloc(1, sizeof(struct sockaddr_in));
    new_reflector->addr->sin_port = htons(62222);

    inet_pton(AF_INET, ip, &new_reflector->addr->sin_addr);
    ht_insert(ht, ip, (char *) new_reflector, sizeof(reflector_t));
}

void load_initial_peers()
{
    char * initial_reflectors[] = {"9.9.9.9", "8.238.120.14", "74.82.42.42", "194.242.2.2", "208.67.222.222", "94.140.14.14"};

    pthread_rwlock_wrlock(&reflector_peers_lock);
    reflector_peers = ht_new_sized(sizeof(initial_reflectors) / sizeof(initial_reflectors[0]));

    for (int i = 0; i < sizeof(initial_reflectors) / sizeof(initial_reflectors[0]); i++) {
        log_debug("Adding initial peer: %s", initial_reflectors[i]);
        add_reflector(initial_reflectors[i], reflector_peers);
    }

    pthread_rwlock_unlock(&reflector_peers_lock);
}

void load_reflector_list(char * path)
{
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(path, "r");
    
    if (fp == NULL)
    {
        log_error("Could not open reflector file: %s", path);
        return;
    }

    int i = 0;

    pthread_rwlock_wrlock(&reflector_pool_lock);
    reflector_pool = ht_new_sized(384);

    while ((read = getline(&line, &len, fp)) != -1) {
        // skip first line
        if (i == 0) {
            i++;
            continue;
        }

        char * ip = strtok(line, ",");
        add_reflector(ip, reflector_pool);
        i++;
    }

    fclose(fp);
    if (line)
        free(line);

    log_info("Reflector pool size: %d", reflector_pool->count);
    pthread_rwlock_unlock(&reflector_pool_lock);
}

void * reflector_peer_selector()
{
    // we start out reselecting every 30 seconds, then after 40 reselections we move to every 15 mins
    struct timespec selector_sleep_time = {.tv_sec = 30};
    struct timespec baseline_sleep_time = {.tv_sec = floor(settings.tick_duration * M_PI),
                                           .tv_nsec = floor(fmod(settings.tick_duration * M_PI, 1) * 1e9)};
    int reselection_count = 0;
    
    // Initial wait of several seconds to allow some OWD data to build up
    nanosleep(&baseline_sleep_time, NULL);

    while (1)
    {
        uint8_t reselect;
        pthread_queue_getmsg(reselector_channel, &reselect, (selector_sleep_time.tv_sec * 1000) + (selector_sleep_time.tv_nsec / 1e6));
        log_info("Starting reselection");

        reselection_count = reselection_count + 1;
        
        if (reselection_count > 40)
            selector_sleep_time.tv_sec = 15 * 60; // 15 mins

        pthread_rwlock_rdlock(&reflector_pool_lock);
        int pool_size = reflector_pool->count;
        

        // remove all reflectors currently in use
        pthread_rwlock_wrlock(&reflector_peers_lock);
        ht_del_hash_table(reflector_peers);
        reflector_peers = ht_new();

        // pick 20 random reflectors and add them to the hash table for some re-baselining
        int target = fmin(20, pool_size);
        int picked = 0;

        while (picked < target)
        {
            int y = rand_in_range(0, pool_size);

            ht_item * item = reflector_pool->items[y];

            if (item == NULL || item == &HT_DELETED_ITEM)
			    continue;

            log_info("Adding reflector %s for baselining", item->key);
            add_reflector(item->key, reflector_peers);
            picked++;
        }

        pthread_rwlock_unlock(&reflector_peers_lock);
        pthread_rwlock_unlock(&reflector_pool_lock);

        // Wait for several seconds to allow all reflectors to be re-baselined
        nanosleep(&baseline_sleep_time, NULL);

        pthread_rwlock_wrlock(&owd_lock);
        pthread_rwlock_wrlock(&reflector_peers_lock);

        for (int i = 0; i < reflector_peers->size; i++)
        {
            ht_item * item = reflector_peers->items[i];

            if (item == NULL || item == &HT_DELETED_ITEM)
                continue;
            
            owd_data_t * data = (owd_data_t *) ht_search(owd_data, item->key);
            reflector_t * reflector = (reflector_t *) item->value;

            if (data)
            {
                int rtt = data->recent_ewma_down + data->recent_ewma_up;

                if (rtt > 0 && rtt < 50000)
                {
                    log_info("Candidate reflector: %s RTT: %d", item->key, rtt);
                    reflector->rtt = rtt;
                }
                else
                {
                    log_info("Dropping candidate with RTT outside thresholds: %s", item->key);
                    ht_delete(reflector_peers, item->key);
                }
            }
            else
            {
                log_info("No data found from candidate reflector: %s - skipping", item->key);
            }
        }

        log_debug("Copying %d reflectors", reflector_peers->count);
        ht_item ** tmp = calloc(reflector_peers->count, sizeof(ht_item*));
        // copy valid item pointers to tmp array for sorting
        for (int i = 0; i < reflector_peers->size; i++)
        {
            ht_item * item = reflector_peers->items[i];

            if (item == NULL || item == &HT_DELETED_ITEM)
                continue;


            reflector_t * reflector = (reflector_t *) item->value;
            log_debug("item key: %s, item value: %p, reflector: %s", item->key, item->value, reflector->ip);
            memcpy(&tmp[i], &item, sizeof(ht_item));
        }

        // sort reflectors according to RTT
        //qsort(tmp, reflector_peers->count, sizeof(ht_item *), rtt_sort);

        // add 5 best reflectors back into new table
        hash_table * new_ht = ht_new_sized(5);
        for (int i = 0; i < 5; i++)
        {
            ht_item * item = tmp[i];
            log_debug("item[%d]: %p", i, item);
            /*ht_insert(new_ht, item->key, item->value, item->size);

            free(item->key);
            free(item->value);
            free(item);*/
        }

        ht_del_hash_table(reflector_peers);
        reflector_peers = new_ht;

        pthread_rwlock_unlock(&owd_lock);
        pthread_rwlock_unlock(&reflector_peers_lock);
    }
}