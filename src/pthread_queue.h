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

/** @file pthread_queue.h
 * @brief pthread FIFO message queue implementation
 */

#ifndef PTHREAD_QUEUE_H
#define PTHREAD_QUEUE_H

#include <stdint.h>

typedef struct pthread_queue_s {
	char		  *	buffer;		/* circular buffer */
	pthread_mutex_t	mutex;		/* lock the structure */
	pthread_cond_t	full;		/* full condition */
	pthread_cond_t	empty;		/* empty condition */
	uint32_t		head;		/* head of queue (first element) */
	uint32_t		tail;		/* tail of queue (last element) */
	uint32_t		count;		/* number of elements in queue */
	uint32_t		qsize;		/* max number of elements in queue */
	uint32_t		msg_len;	/* length of each message */
	uint8_t			reset;		/* 0 = not reset, otherwise reset */
	uint8_t			destroyFree;/* 1 = free memory on destroy */
} pthread_queue_t;


/** Create a message queue with fixed length messages.
 *
 * Set *ppqueue = NULL to allocate memory for the queue. Otherwise, caller allocates memory.
 *
 * @param[inout] ppqueue		if *ppqueue == NULL, allocate memory for queue. Returns queue pointer.
 * @param[in]	 qstart			pointer to the queue buffer
 * @param[in]    num_msg        maximum number of messages in the queue
 * @param[in]    msg_len_bytes  maximum size of each message in bytes
 * @returns                   0 for success, otherwise an error number for failure
 * @ERRORS
 *      [ENOMEM]            	memory for queue not available
 */
int pthread_queue_create(pthread_queue_t ** ppqueue, void * qstart, uint32_t num_msg, uint32_t msg_len_bytes);



/** Destroy a message queue.
 * 
 * @param[in]  queue          pointer to the queue to destroy
 * @returns                   nothing
 */
void pthread_queue_destroy(pthread_queue_t *queue);



/** Send message to a queue.
 * 
 * Message is copied into the queue. Calling function can deallocate local copy of
 * message immediately after function returns.
 *
 * If the queue is full, and timeout == PTHREAD_NOWAIT, function returns immediately
 * with return value of -1. If timeout == PTHREAD_WAIT, function waits indefinitely for
 * space to become available in the queue. Otherwise, if timeout is a positive value > 0,
 * the function waits for <timeout> ms for space to become available.
 *
 * @param[in] queue         pointer to the queue
 * @param[in] msg           message to place in the queue
 * @param[in] timeout       PTHREAD_WAIT or PTHREAD_NOWAIT or timeout in ms
 * @returns                 0 for success, otherwise an error number for failure
 * @ERRORS
 *      [ETIMEDOUT]         timeout has passed (or, if PTHREAD_NOWAIT, queue is full)
 *      [EINVAL]            timeout value is invalid
 *      [ECANCELED]         queue was reset, message was not put in queue
 */
int pthread_queue_sendmsg(pthread_queue_t *queue, void *msg, long timeout);



/** Get message from a queue.
 *
 * Message is copied from the queue. Calling function can deallocate local copy of
 * message whenever it has finished with the data.The buffer pointed to by msg must
 * be at least as large as the message size specified when the queue was created
 * (see msg_len_bytes arg in pthread_queue_create).
 *
 * If the queue is empty, and timeout == PTHREAD_NOWAIT, function returns immediately
 * with return value of ETIMEDOUT. If timeout == PTHREAD_WAIT, function waits indefinitely for
 * a message to become available in the queue. Otherwise, if timeout is a positive value > 0,
 * the function waits for <timeout> ms for a message to become available.
 *
 * @param[in]  queue		pointer to the queue
 * @param[out] msg			buffer to receive message from the queue.
 * @param[in]  timeout		PTHREAD_WAIT or PTHREAD_NOWAIT or timeout in ms
 * @returns                 0 for success, otherwise an error number for failure
 * @ERRORS
 *      [ETIMEDOUT]         timeout has passed (or, if PTHREAD_NOWAIT, queue is full)
 *      [EINVAL]            timeout value is invalid
 *      [ECANCELED]         queue was reset
 */
int pthread_queue_getmsg(pthread_queue_t *queue, void *msg, long timeout);



/** Return number of messages in a queue.
 *
 * @param[in] queue			pointer to the queue
 */
uint32_t pthread_queue_count(pthread_queue_t * queue);



/** Reset queue, discarding all messages, prevent further message inputs.
 *
 * @param[in] queue			pointer to the queue
 */
int pthread_queue_reset(pthread_queue_t * queue);



/** Unreset queue, allow message inputs.
 *
 * @param[in] queue			pointer to the queue
 */
int pthread_queue_unreset(pthread_queue_t * queue);

#endif /* PTHREAD_QUEUE_H */
