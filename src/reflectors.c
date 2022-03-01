#define _GNU_SOURCE
#include <math.h>

#include "globals.h"

void * reflector_peer_selector()
{
    // we start out reselecting every 30 seconds, then after 40 reselections we move to every 15 mins
    struct timespec selector_sleep_time = {.tv_sec = 30};
    struct timespec baseline_sleep_time = {.tv_sec = floor(tick_duration * M_PI),
                                           .tv_nsec = floor(fmod(tick_duration * M_PI, 1) * 1e9)};
    int reselection_count = 0;
    
    // Initial wait of several seconds to allow some OWD data to build up
    nanosleep(&baseline_sleep_time, NULL);

    while (1)
    {
        uint8_t reselect;
        pthread_queue_getmsg(reselector_channel, &reselect, (selector_sleep_time.tv_sec * 1000) + (selector_sleep_time.tv_nsec / 1e6));
        
        reselection_count = reselection_count + 1;
        
        if (reselection_count > 40)
            selector_sleep_time.tv_sec = 15 * 60; // 15 mins

        
    }
}