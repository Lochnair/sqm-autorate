#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>

#include "globals.h"

#define rand_in_range(min, max) rand() % (max + 1 - min) + min

int rtt_sort(void * a, void * b)
{
    reflector_t * refl_a = (reflector_t *) a;
    reflector_t * refl_b = (reflector_t *) b;
    
    return refl_a->rtt - refl_b->rtt;
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
        printf("[resel] Starting reselection\n");

        reselection_count = reselection_count + 1;
        
        if (reselection_count > 40)
            selector_sleep_time.tv_sec = 15 * 60; // 15 mins

        pthread_rwlock_rdlock(&reflector_pool_lock);
        int pool_size = HASH_COUNT(reflector_pool);
        reflector_t pool[pool_size];

        reflector_t * pool_entry;

        // copy pool entries into an array too more easily pick random entries
        int i = 0;
        for (pool_entry = reflector_pool; pool_entry != NULL; pool_entry = pool_entry->hh.next)
        {
            pool[i] = *pool_entry;
            i++;
        }

        pthread_rwlock_unlock(&reflector_pool_lock);

        // remove all reflectors currently in use
        pthread_rwlock_wrlock(&reflector_peers_lock);
        reflector_t * current_reflector, * tmp;

        HASH_ITER(hh, reflector_peers, current_reflector, tmp) {
            printf("[resel] Removing reflector %s\n", current_reflector->ip);
            HASH_DEL(reflector_peers, current_reflector);

            if (current_reflector->addr)
                free(current_reflector->addr);

            free(current_reflector);
        }

        // pick 20 random reflectors and add them to the hash table for some re-baselining
        for (int x = 0; x < 20; x++)
        {
            reflector_t * candidate = calloc(1, sizeof(reflector_t));
            int y = rand_in_range(0, pool_size);
            strncpy(candidate->ip, (char *) &(pool[y]).ip, INET_ADDRSTRLEN);
            char * ip = calloc(1, INET_ADDRSTRLEN + 1);
            strncpy(ip, (char *) &(pool[y]).ip, INET_ADDRSTRLEN);


            printf("[resel] Adding reflector %s for baselining\n", ip);
            HASH_ADD_STR(reflector_peers, ip, &pool[y]);
        }

        pthread_rwlock_unlock(&reflector_peers_lock);

        // Wait for several seconds to allow all reflectors to be re-baselined
        nanosleep(&baseline_sleep_time, NULL);

        pthread_rwlock_wrlock(&owd_lock);

        HASH_ITER(hh, reflector_peers, current_reflector, tmp) {
            owd_data_t * recent;
            HASH_FIND_STR(owd_recent, current_reflector->ip, recent);

            if (recent)
            {
                int rtt = recent->down_ewma + recent->up_ewma;

                if (rtt > 0 && rtt < 50000)
                {
                    printf("[resel] Candidate reflector: %s RTT: %d\n", current_reflector->ip, rtt);
                    current_reflector->rtt = rtt;
                }
                else
                {
                    printf("[resel] Dropping candidate with RTT outside thresholds: %s\n", current_reflector->ip);
                    HASH_DEL(reflector_peers, current_reflector);
                }
            }
            else
            {
                printf("[resel] No data found from candidate reflector: %s - skipping\n", current_reflector->ip);
            }
        }

        HASH_SORT(reflector_peers, rtt_sort);

        // TODO: Shuffle the reflectors to avoid overwhelming the good ones
        // Keep only the best 5 reflectors
        int del_i = 0;
        HASH_ITER(hh, reflector_peers, current_reflector, tmp) {

            if(del_i < 5)
            {
                printf("[resel] Fastest candidate %s: %d\n", current_reflector->ip, current_reflector->rtt);
            }
            else
            {
                printf("[resel] Removing candidate reflector %s\n", current_reflector->ip);
                HASH_DEL(reflector_peers, current_reflector);

            }

            del_i++;
        }

        pthread_rwlock_unlock(&owd_lock);
    }
}