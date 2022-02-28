/*
The MIT License (MIT)

Copyright (c) 2014, Stephen Scott
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

/* 
 * pthread_queue implementation
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "pthread_queue.h"
#include "pthread_ext_common.h"

/**************************************************************************************************/
static void cleanup_handler(void *arg)
{
	pthread_mutex_unlock((pthread_mutex_t*)arg);
}

/**************************************************************************************************/
/* pthread_queue_create
 * create and initialize a new queue.
 */
int pthread_queue_create(pthread_queue_t ** ppqueue, void * qstart, uint32_t num_msg, uint32_t msg_len_bytes)
{
	pthread_queue_t * queue;

	if (NULL == *ppqueue)
	{
		queue = (pthread_queue_t *) malloc(sizeof(pthread_queue_t));
		if (NULL == queue)
			return ENOMEM;
	
		queue->buffer = (char *) malloc(num_msg * msg_len_bytes);
		if (NULL == queue->buffer)
		{
			free(queue);
			return ENOMEM;
		}

		*ppqueue = queue;
		queue->destroyFree = 1;
	}
	else
	{
		queue = *ppqueue;
		queue->buffer = (char *) qstart;
		if (NULL == queue->buffer)
		{
			return ENOMEM;
		}
		queue->destroyFree = 0;
	}

	pthread_mutex_init(&queue->mutex, NULL);
	pthread_cond_init(&queue->full, NULL);
	pthread_cond_init(&queue->empty, NULL);
	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
	queue->qsize = num_msg;
	queue->msg_len = msg_len_bytes;
	queue->reset = 0;

	return 0;
}

/**************************************************************************************************/
/* pthread_queue_destroy
 * free a queue.
 */
void pthread_queue_destroy(pthread_queue_t *queue)
{
	pthread_mutex_destroy(&queue->mutex);
	pthread_cond_destroy(&queue->full);
	pthread_cond_destroy(&queue->empty);
	if (queue->destroyFree)
	{
		free(queue->buffer);
		free(queue);
	}

} /* pthread_queue_destroy */


/**************************************************************************************************/
/* pthread_queue_sendmsg
 * puts new message on the queue.
 * timeout is PTHREAD_WAIT or PTHREAD_NOWAIT or timeout in ms
 * If timeout == PTHREAD_WAIT and the queue is full, function waits until there is room.
 * If blocked on full queue, function wakes up if queue is reset, and returns ECANCELED.
 */
int pthread_queue_sendmsg(pthread_queue_t *queue, void *msg, long timeout)
{
	struct timespec abstime;
	uint8_t			reset;
	int				result;

	if ( (PTHREAD_WAIT != timeout) && (timeout < 0) )
		return EINVAL;

	// convert wait to absolute system time
	if (timeout > 0)
		pthread_ext_ms2abs_time(timeout, &abstime);

	pthread_mutex_lock(&queue->mutex);

	/* handle nowait and queue is full */
	if ( (PTHREAD_NOWAIT == timeout) && (queue->count == queue->qsize) )
	{
		pthread_mutex_unlock(&queue->mutex);
		return ETIMEDOUT;
	}

	/* wait while buffer full */
	while ((queue->count == queue->qsize) && !queue->reset) {

		pthread_cleanup_push(cleanup_handler, &queue->mutex);
		if (PTHREAD_WAIT == timeout)
			result = pthread_cond_wait(&queue->full, &queue->mutex);
		else
			result = pthread_cond_timedwait(&queue->full, &queue->mutex, &abstime);
		pthread_cleanup_pop(0);

		if (ETIMEDOUT == result)
		{
			pthread_mutex_unlock(&queue->mutex);
			return ETIMEDOUT;
		}
	}

	reset = queue->reset;	// set this in critical section so we can look at
							// it after we unlock the mutex
	if (!reset)
	{
		/* copy message to queue */
		memcpy(&queue->buffer[queue->tail * queue->msg_len], msg, queue->msg_len);
		queue->count += 1;
		queue->tail = (queue->tail == queue->qsize-1) ? 0 : queue->tail+1;
	}

	/* signal waiting consumer */
	pthread_mutex_unlock(&queue->mutex);
	if (!reset)
		pthread_cond_signal(&queue->empty);

	result = reset ? ECANCELED : 0;

	return result;

} /* pthread_queue_sendmsg */


/**************************************************************************************************/
/* pthread_queue_getmsg
 * gets the oldest message in the queue.
 * timeout is PTHREAD_WAIT or PTHREAD_NOWAIT or timeout in ms
 * If timeout == PTHREAD_WAIT and the queue is full, function waits until there is room.
 */

int pthread_queue_getmsg(pthread_queue_t *queue, void *msg, long timeout)
{
	struct timespec abstime;

	if ( (PTHREAD_WAIT != timeout) && (timeout < 0) )
		return EINVAL;

	// convert wait to absolute system time
	if (timeout > 0)
		pthread_ext_ms2abs_time(timeout, &abstime);

	pthread_mutex_lock(&queue->mutex);

	/* handle nowait and queue is empty */
	if ( (PTHREAD_NOWAIT == timeout) && (queue->count == 0) )
	{
		pthread_mutex_unlock(&queue->mutex);
		return ETIMEDOUT;
	}

	/* wait while there is nothing in the buffer */
	while (queue->count == 0) {
		int result;

		pthread_cleanup_push(cleanup_handler, &queue->mutex);
		if (PTHREAD_WAIT == timeout)
			result = pthread_cond_wait(&queue->empty, &queue->mutex);
		else
			result = pthread_cond_timedwait(&queue->empty, &queue->mutex, &abstime);
		pthread_cleanup_pop(0);

		if (ETIMEDOUT == result)
		{
			pthread_mutex_unlock(&queue->mutex);
			return ETIMEDOUT;
		}
	}

	/* copy message from the queue */
	memcpy(msg, &queue->buffer[queue->head * queue->msg_len], queue->msg_len);
	queue->count--;
	queue->head = (queue->head == queue->qsize-1) ? 0 : queue->head+1;

	/* signal waiting producer */
	pthread_mutex_unlock(&queue->mutex);
	pthread_cond_signal(&queue->full);

	return (0);

} /* pthread_queue_getmsg */

/**************************************************************************************************/
/* pthread_queue_count
 * return number of entries in queue
 */
uint32_t pthread_queue_count(pthread_queue_t * queue)
{
	return queue->count;
}
/**************************************************************************************************/
/* pthread_queue_reset
 * clear the queue and prevent any inputs while reset. Wake up all threads
 * blocked on a full message queue and force them to stop waiting.
 */
int pthread_queue_reset(pthread_queue_t * queue)
{
	pthread_mutex_lock(&queue->mutex);
	queue->head = 0;
	queue->tail = 0;
	queue->count = 0;
	queue->reset = 1;
	pthread_mutex_unlock(&queue->mutex);
	pthread_cond_broadcast(&queue->full);

	return 0;
}

/**************************************************************************************************/
/* pthread_queue_unreset
 * reenable the queue
 */
int pthread_queue_unreset(pthread_queue_t * queue)
{
	pthread_mutex_lock(&queue->mutex);
	queue->reset = 0;
	pthread_mutex_unlock(&queue->mutex);

	return 0;
}
