// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include "funnel-requestqueue.h"

#include <linux/atomic.h>
#include <linux/cache.h>

#include "event-count.h"
#include "funnel-queue.h"
#include "logger.h"
#include "memory-alloc.h"
#include "thread-utils.h"
#include "time-utils.h"

/*
 * This queue will attempt to handle requests in reasonably sized batches
 * instead of reacting immediately to each new request. The wait time between
 * batches is dynamically adjusted up or down to try to balance responsiveness
 * against wasted thread run time.
 *
 * If the wait time becomes long enough, the queue will become dormant and must
 * be explicitly awoken when a new request is enqueued. The enqueue operation
 * updates "newest" in the funnel queue via xchg (which is a memory barrier),
 * and later checks "dormant" to decide whether to do a wakeup of the worker
 * thread.
 *
 * When deciding to go to sleep, the worker thread sets "dormant" and then
 * examines "newest" to decide if the funnel queue is idle. In dormant mode,
 * the last examination of "newest" before going to sleep is done inside the
 * wait_event_interruptible macro(), after a point where one or more memory
 * barriers have been issued. (Preparing to sleep uses spin locks.) Even if the
 * funnel queue's "next" field update isn't visible yet to make the entry
 * accessible, its existence will kick the worker thread out of dormant mode
 * and back into timer-based mode.
 *
 * Unbatched requests are used to communicate between different zone threads
 * and will also cause the queue to awaken immediately.
 */

enum {
	NANOSECOND = 1,
	MICROSECOND = 1000 * NANOSECOND,
	MILLISECOND = 1000 * MICROSECOND,
	DEFAULT_WAIT_TIME = 10 * MICROSECOND,
	MINIMUM_WAIT_TIME = DEFAULT_WAIT_TIME / 2,
	MAXIMUM_WAIT_TIME = MILLISECOND,
	MINIMUM_BATCH = 32,
	MAXIMUM_BATCH = 64,
};

struct uds_request_queue {
	/* The name of queue */
	const char *name;
	/* Function to process a request */
	uds_request_queue_processor_fn processor;
	/* Queue of new incoming requests */
	struct funnel_queue *main_queue;
	/* Queue of old requests to retry */
	struct funnel_queue *retry_queue;
	/* Signal to wake the worker thread */
	struct event_count *work_event;
	/* The thread id of the worker thread */
	struct thread *thread;
	/* True if the worker was started */
	bool started;
	/* When true, requests can be enqueued */
	bool running;
	/* A flag set when the worker is waiting without a timeout */
	atomic_t dormant;

	/*
	 * The following fields are mutable state private to the worker thread.
	 * The first field is aligned to avoid cache line sharing with
	 * preceding fields.
	 */

	/* Requests processed since last wait */
	uint64_t current_batch __aligned(L1_CACHE_BYTES);
	/* The amount of time to wait to accumulate a batch of requests */
	uint64_t wait_nanoseconds;
	/* The relative time at which to wake when waiting with a timeout */
	ktime_t wake_rel_time;
};

/**********************************************************************/
static void adjust_wait_time(struct uds_request_queue *queue)
{
	uint64_t delta = queue->wait_nanoseconds / 4;

	if (queue->current_batch < MINIMUM_BATCH)
		queue->wait_nanoseconds += delta;
	else if (queue->current_batch > MAXIMUM_BATCH)
		queue->wait_nanoseconds -= delta;
}

/**
 * Decide if the queue should wait with a timeout or enter the dormant mode
 * of waiting without a timeout. If timing out, returns an relative wake
 * time to pass to the wait call, otherwise returns NULL. (wake_rel_time is a
 * queue field to make it easy for this function to return NULL).
 *
 * @param queue  the request queue
 *
 * @return a pointer the relative wake time, or NULL if there is no timeout
 **/
static ktime_t *get_wake_time(struct uds_request_queue *queue)
{
	if (queue->wait_nanoseconds >= MAXIMUM_WAIT_TIME) {
		if (atomic_read(&queue->dormant)) {
			/* The thread is going dormant. */
			queue->wait_nanoseconds = DEFAULT_WAIT_TIME;
			return NULL;
		}

		queue->wait_nanoseconds = MAXIMUM_WAIT_TIME;
		atomic_set_release(&queue->dormant, true);
	} else if (queue->wait_nanoseconds < MINIMUM_WAIT_TIME) {
		queue->wait_nanoseconds = MINIMUM_WAIT_TIME;
	}

	queue->wake_rel_time = queue->wait_nanoseconds;
	return &queue->wake_rel_time;
}

/**
 * Poll the underlying lock-free queues for a request to process. Requests in
 * the retry queue have higher priority, so that queue is polled first.
 *
 * @param queue  the request queue being serviced
 *
 * @return a dequeued request, or NULL if no request was available
 **/
static struct uds_request *poll_queues(struct uds_request_queue *queue)
{
	struct funnel_queue_entry *entry;

	entry = vdo_funnel_queue_poll(queue->retry_queue);
	if (entry != NULL)
		return container_of(entry, struct uds_request, queue_link);

	entry = vdo_funnel_queue_poll(queue->main_queue);
	if (entry != NULL)
		return container_of(entry, struct uds_request, queue_link);

	return NULL;
}

/*
 * Remove the next request to be processed from the queue, waiting for a
 * request if necessary.
 */
static struct uds_request *dequeue_request(struct uds_request_queue *queue)
{
	for (;;) {
		struct uds_request *request;
		event_token_t wait_token;
		ktime_t *wake_time;
		bool shutting_down;

		queue->current_batch++;
		request = poll_queues(queue);
		if (request != NULL)
			return request;

		/* Prepare to wait for more work to arrive. */
		wait_token = event_count_prepare(queue->work_event);

		shutting_down = !READ_ONCE(queue->running);
		if (shutting_down)
			/*
			 * Ensure that we see any remaining requests that were
			 * enqueued before shutting down. The corresponding
			 * write barrier is in uds_request_queue_finish().
			 */
			smp_rmb();

		/*
		 * Poll again in case a request was enqueued just before we got
		 * the event key.
		 */
		request = poll_queues(queue);
		if ((request != NULL) || shutting_down) {
			event_count_cancel(queue->work_event, wait_token);
			return request;
		}

		/* Wait for more work to arrive. */
		adjust_wait_time(queue);
		wake_time = get_wake_time(queue);
		event_count_wait(queue->work_event, wait_token, wake_time);

		if (wake_time == NULL) {
			/*
			 * The queue has been roused from dormancy. Clear the
			 * flag so enqueuers can stop broadcasting. No fence is
			 * needed for this transition.
			 */
			atomic_set(&queue->dormant, false);
			queue->wait_nanoseconds = DEFAULT_WAIT_TIME;
		}

		queue->current_batch = 0;
	}
}

/**********************************************************************/
static void request_queue_worker(void *arg)
{
	struct uds_request_queue *queue = (struct uds_request_queue *) arg;
	struct uds_request *request;

	vdo_log_debug("%s queue starting", queue->name);
	while ((request = dequeue_request(queue)) != NULL)
		queue->processor(request);
	vdo_log_debug("%s queue done", queue->name);
}

/**********************************************************************/
int uds_make_request_queue(const char *queue_name,
			   uds_request_queue_processor_fn processor,
			   struct uds_request_queue **queue_ptr)
{
	int result;
	struct uds_request_queue *queue;

	result = vdo_allocate(1, struct uds_request_queue, __func__, &queue);
	if (result != VDO_SUCCESS)
		return result;

	queue->name = queue_name;
	queue->processor = processor;
	queue->running = true;
	queue->current_batch = 0;
	queue->wait_nanoseconds = DEFAULT_WAIT_TIME;

	result = vdo_make_funnel_queue(&queue->main_queue);
	if (result != UDS_SUCCESS) {
		uds_request_queue_finish(queue);
		return result;
	}

	result = vdo_make_funnel_queue(&queue->retry_queue);
	if (result != UDS_SUCCESS) {
		uds_request_queue_finish(queue);
		return result;
	}

	result = make_event_count(&queue->work_event);
	if (result != UDS_SUCCESS) {
		uds_request_queue_finish(queue);
		return result;
	}

	result = vdo_create_thread(request_queue_worker,
				   queue,
				   queue_name,
				   &queue->thread);
	if (result != VDO_SUCCESS) {
		uds_request_queue_finish(queue);
		return result;
	}

	queue->started = true;
	smp_mb();
	*queue_ptr = queue;
	return UDS_SUCCESS;
}

/**********************************************************************/
static inline void wake_up_worker(struct uds_request_queue *queue)
{
	event_count_broadcast(queue->work_event);
}

/**********************************************************************/
void uds_request_queue_enqueue(struct uds_request_queue *queue,
			       struct uds_request *request)
{
	struct funnel_queue *sub_queue;
	bool unbatched = request->unbatched;

	sub_queue = request->requeued ? queue->retry_queue : queue->main_queue;
	vdo_funnel_queue_put(sub_queue, &request->queue_link);

	/*
	 * We must wake the worker thread when it is dormant. A read fence
	 * isn't needed here since we know the queue operation acts as one.
	 */
	if (atomic_read(&queue->dormant) || unbatched)
		wake_up_worker(queue);
}

/**********************************************************************/
void uds_request_queue_finish(struct uds_request_queue *queue)
{
	if (queue == NULL)
		return;

	/*
	 * This memory barrier ensures that any requests we queued will be
	 * seen. The point is that when dequeue_request() sees the following
	 * update to the running flag, it will also be able to see any change
	 * we made to a next field in the funnel queue entry. The corresponding
	 * read barrier is in dequeue_request().
	 */
	smp_wmb();
	WRITE_ONCE(queue->running, false);

	if (queue->started) {
		wake_up_worker(queue);
		vdo_join_threads(queue->thread);
	}

	free_event_count(queue->work_event);
	vdo_free_funnel_queue(queue->main_queue);
	vdo_free_funnel_queue(queue->retry_queue);
	vdo_free(queue);
}
