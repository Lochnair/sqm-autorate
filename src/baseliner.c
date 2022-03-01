#include "baseliner.h"
#include "utils.h"

#include <arpa/inet.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define __USE_UNIX98
#include <pthread.h>
#include "pthread_ext_common.h"
#include "pthread_queue.h"

#include "globals.h"

void owd_add_reflector(char * reflector) {
	owd_data_t * baseline, * recent;

	baseline = calloc(1, sizeof(owd_data_t));
	recent = calloc(1, sizeof(owd_data_t));

	strcpy((char * restrict) &baseline->reflector, reflector);
	strcpy((char * restrict) &recent->reflector, reflector);

	pthread_rwlock_wrlock(&owd_lock);
	HASH_ADD_STR(owd_baseline, reflector, baseline);
	HASH_ADD_STR(owd_recent, reflector, recent);
	pthread_rwlock_unlock(&owd_lock);
}

void * baseliner_loop()
{
	reflector_t * reflector;
	int reselection_trigger = 1;
	double slow_factor = ewma_factor(tick_duration, 135);
	double fast_factor = ewma_factor(tick_duration, 0.4);

	pthread_rwlock_rdlock(&reflectors_lock);
	for (reflector = reflectors; reflector != NULL; reflector = reflector->hh.next) {
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &reflector->addr->sin_addr, (char * restrict) &ip, INET_ADDRSTRLEN);
		printf("adding %s\n", ip);
		owd_add_reflector((char *) &ip);
	}
	pthread_rwlock_unlock(&reflectors_lock);

	while (1)
	{
		time_data_t * time_data = calloc(1, sizeof(time_data_t));
		
		int ret = pthread_queue_getmsg(baseliner_queue, time_data, PTHREAD_WAIT);

		if (ret != 0)
			continue;

		owd_data_t * baseline = NULL, * recent = NULL;

		printf("looking for %s\n", time_data->reflector);

		/*HASH_ITER(hh, owd_baseline, baseline, tmp) {
			printf("refl iter: %s, down_ewma: %f\n", baseline->reflector, baseline->down_ewma);
		}*/

		pthread_rwlock_rdlock(&owd_lock);
		HASH_FIND_STR(owd_baseline, time_data->reflector, baseline);
		HASH_FIND_STR(owd_recent, time_data->reflector, recent);

		if (!baseline || !recent)
		{
			pthread_rwlock_unlock(&owd_lock);
			add_reflector((char * ) time_data->reflector);
			pthread_rwlock_rdlock(&owd_lock);
		}

		if (!baseline->down_ewma)
			baseline->down_ewma = time_data->downlink_time;

		if (!baseline->up_ewma)
			baseline->up_ewma = time_data->uplink_time;

		if (!baseline->last_receive_time_s)
			baseline->last_receive_time_s = time_data->last_receive_time_s;

		if (!recent->down_ewma)
			recent->down_ewma = time_data->downlink_time;

		if (!recent->up_ewma)
			recent->up_ewma = time_data->uplink_time;

		if (!recent->last_receive_time_s)
			recent->last_receive_time_s = time_data->last_receive_time_s;

		if (time_data->last_receive_time_s - baseline->last_receive_time_s > 30 || time_data->last_receive_time_s - recent->last_receive_time_s > 30)
		{
			/*
			 * This reflector is out of date, it's probably newly chosen from the
			 * choice cycle, reset all the ewmas to the current value
			 */
			baseline->down_ewma = time_data->downlink_time;
			baseline->up_ewma = time_data->uplink_time;
			recent->down_ewma = time_data->downlink_time;
			recent->up_ewma = time_data->uplink_time;
		}

		// if this reflection is more than 5 seconds higher than baseline.. mark it no good and trigger a reselection
		if (time_data->uplink_time > baseline->up_ewma + 5000 || time_data->downlink_time > baseline->down_ewma + 5000)
		{
			// 5000 ms is a weird amount of time for a ping. let's mark this old and no good
			baseline->last_receive_time_s = time_data->last_receive_time_s - 60;
			recent->last_receive_time_s = time_data->last_receive_time_s - 60;
			
			// trigger a reselection of reflectors here
			pthread_queue_sendmsg(reselector_channel, &reselection_trigger, PTHREAD_NOWAIT);
		}
		else
		{
			baseline->down_ewma = baseline->down_ewma * slow_factor  + (1 - slow_factor) * time_data->downlink_time;
			baseline->up_ewma = baseline->up_ewma * slow_factor  + (1 - slow_factor) * time_data->uplink_time;
			recent->down_ewma = recent->down_ewma * fast_factor + (1 - fast_factor) * time_data->downlink_time;
			recent->up_ewma = recent->up_ewma * fast_factor + (1 - fast_factor) * time_data->uplink_time;

			// when baseline is above the recent, set equal to recent, so we track more quickly
			baseline->down_ewma = fmin(baseline->down_ewma, recent->down_ewma);
			baseline->up_ewma = fmin(baseline->up_ewma, recent->up_ewma);
		}

		printf("Reflector %s up baseline = %f down baseline = %f\n", time_data->reflector, baseline->up_ewma, baseline->down_ewma);
		printf("Reflector %s up recent = %f down recent = %f\n", time_data->reflector, recent->up_ewma, recent->down_ewma);

		pthread_rwlock_unlock(&owd_lock);
		free(time_data);
	}

	return 0;
}