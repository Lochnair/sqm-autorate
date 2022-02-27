#include "baseliner.h"
#include "utils.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#define __USE_UNIX98
#include <pthread.h>
#include "pthread_ext_common.h"
#include "pthread_queue.h"

#include "globals.h"

void add_reflector(char * reflector) {
	owd_data_t * baseline, * recent;

	baseline = calloc(1, sizeof(owd_data_t));
	recent = calloc(1, sizeof(owd_data_t));

	strcpy(&baseline->reflector, reflector);
	strcpy(&recent->reflector, reflector);

	baseline->down_ewma = 5;

	pthread_rwlock_wrlock(&owd_lock);
	HASH_ADD_STR(owd_baseline, reflector, baseline);
	HASH_ADD_STR(owd_recent, reflector, recent);
	pthread_rwlock_unlock(&owd_lock);
}

void * baseliner_loop()
{
	reflector_t * reflector;
	double slow_factor = ewma_factor(tick_duration, 135);
	double fast_factor = ewma_factor(tick_duration, 0.4);

	pthread_rwlock_rdlock(&reflectors_lock);
	for (reflector = reflectors; reflector != NULL; reflector = reflector->hh.next) {
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &reflector->addr->sin_addr, &ip, INET_ADDRSTRLEN);
		printf("adding %s\n", ip);
		add_reflector(&ip);
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

		if (baseline)
		{
			printf("refl: %s, down_ewma: %f\n", time_data->reflector, baseline->down_ewma);
		}
		
		if (recent)
		{
			printf("refl: %s, down_ewma: %f\n", time_data->reflector, recent->down_ewma);
		}

		pthread_rwlock_unlock(&owd_lock);
		free(time_data);
	}

	return 0;
}