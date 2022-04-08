#include "baseliner.h"
#include "utils.h"

#include <arpa/inet.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"

hash_table * owd_data;

void owd_add_reflector(char * reflector) {
	owd_data_t * entry = calloc(1, sizeof(owd_data_t));
	strncpy(entry->reflector, reflector, INET_ADDRSTRLEN);

	ht_insert(owd_data, reflector, (char *) entry, sizeof(owd_data_t));
}

void * baseliner_loop()
{
	reflector_t * reflector;
	int reselection_trigger = 1;
	double slow_factor = ewma_factor(settings.tick_duration, 135);
	double fast_factor = ewma_factor(settings.tick_duration, 0.4);

	pthread_rwlock_rdlock(&reflector_peers_lock);
	pthread_rwlock_wrlock(&owd_lock);

	owd_data = ht_new();

	for (int i = 0; i < reflector_peers->size; i++)
	{
		ht_item * item = reflector_peers->items[i];

		if (item == NULL || item == &HT_DELETED_ITEM)
			continue;
		
		reflector = (reflector_t *) item->value;
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &reflector->addr->sin_addr, (char * restrict) &ip, INET_ADDRSTRLEN);
		log_info("Initializing OWD data for reflector: %s", ip);
		owd_add_reflector((char *) &ip);
	}

	pthread_rwlock_unlock(&owd_lock);
	pthread_rwlock_unlock(&reflector_peers_lock);

	while (1)
	{
		time_data_t * time_data = calloc(1, sizeof(time_data_t));
		
		int ret = pthread_queue_getmsg(baseliner_queue, time_data, PTHREAD_WAIT);

		if (ret != 0)
			continue;

		pthread_rwlock_wrlock(&owd_lock);

		owd_data_t * data = (owd_data_t *) ht_search(owd_data, time_data->reflector);

		if (!data)
		{
			owd_add_reflector((char * ) time_data->reflector);

			data = (owd_data_t *) ht_search(owd_data, time_data->reflector);
		}

		if (!data->baseline_ewma_down)
			data->baseline_ewma_down = time_data->downlink_time;

		if (!data->baseline_ewma_up)
			data->baseline_ewma_up = time_data->uplink_time;

		if (!data->recent_ewma_down)
			data->recent_ewma_down = time_data->downlink_time;

		if (!data->recent_ewma_up)
			data->recent_ewma_up = time_data->uplink_time;

		if (!data->last_receive_time_s)
			data->last_receive_time_s = time_data->last_receive_time_s;

		if (time_data->last_receive_time_s - data->last_receive_time_s > 30)
		{
			/*
			 * This reflector is out of date, it's probably newly chosen from the
			 * choice cycle, reset all the ewmas to the current value
			 */
			data->baseline_ewma_down = time_data->downlink_time;
			data->baseline_ewma_up = time_data->uplink_time;
			data->recent_ewma_down = time_data->downlink_time;
			data->recent_ewma_up = time_data->uplink_time;
		}

		// if this reflection is more than 5 seconds higher than baseline.. mark it no good and trigger a reselection
		if (time_data->uplink_time > data->baseline_ewma_up + 5000 || time_data->downlink_time > data->baseline_ewma_down + 5000)
		{
			// 5000 ms is a weird amount of time for a ping. let's mark this old and no good
			data->last_receive_time_s = time_data->last_receive_time_s - 60;
			
			// trigger a reselection of reflectors here
			pthread_queue_sendmsg(reselector_channel, &reselection_trigger, PTHREAD_NOWAIT);
		}
		else
		{
			data->baseline_ewma_down = data->baseline_ewma_down * slow_factor  + (1 - slow_factor) * time_data->downlink_time;
			data->baseline_ewma_up = data->baseline_ewma_up * slow_factor  + (1 - slow_factor) * time_data->uplink_time;
			data->recent_ewma_down = data->recent_ewma_down * fast_factor + (1 - fast_factor) * time_data->downlink_time;
			data->recent_ewma_up = data->recent_ewma_up * fast_factor + (1 - fast_factor) * time_data->uplink_time;

			// when baseline is above the recent, set equal to recent, so we track more quickly
			data->baseline_ewma_down = fmin(data->baseline_ewma_down, data->recent_ewma_down);
			data->baseline_ewma_up = fmin(data->baseline_ewma_up, data->recent_ewma_up);
		}

		log_debug("Reflector %s up baseline = %f down baseline = %f", time_data->reflector, data->baseline_ewma_up, data->baseline_ewma_down);
		log_debug("Reflector %s up recent = %f down recent = %f", time_data->reflector, data->recent_ewma_up, data->recent_ewma_down);

		pthread_rwlock_unlock(&owd_lock);
		free(time_data);
	}

	return 0;
}