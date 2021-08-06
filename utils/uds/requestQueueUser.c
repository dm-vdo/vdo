/*
 * Copyright Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA. 
 *
 * $Id: //eng/uds-releases/krusty-rhel9.0-beta/userLinux/uds/requestQueueUser.c#1 $
 */

#include "requestQueue.h"

#include "atomicDefs.h"
#include "compiler.h"
#include "logger.h"
#include "request.h"
#include "memoryAlloc.h"
#include "uds-threads.h"
#include "timeUtils.h"
#include "util/eventCount.h"
#include "util/funnelQueue.h"

/*
 * Ordering:
 *
 * Multiple retry requests or multiple non-retry requests enqueued from
 * a single producer thread will be processed in the order enqueued.
 *
 * Retry requests will generally be processed before normal requests.
 *
 * HOWEVER, a producer thread can enqueue a retry request (generally given
 * higher priority) and then enqueue a normal request, and they can get
 * processed in the reverse order.  The checking of the two internal queues is
 * very simple and there's a potential race with the producer regarding the
 * "priority" handling.  If an ordering guarantee is needed, it can be added
 * without much difficulty, it just makes the code a bit more complicated.
 *
 * If requests are enqueued while the processing of another request is
 * happening, and the enqueuing operations complete while the request
 * processing is still in progress, then the retry request(s) *will*
 * get processed next.  (This is used for testing.)
 */

/**
 * Time constants, all in units of nanoseconds.
 **/
enum {
	ONE_NANOSECOND = 1,
	ONE_MICROSECOND = 1000 * ONE_NANOSECOND,
	ONE_MILLISECOND = 1000 * ONE_MICROSECOND,
	ONE_SECOND = 1000 * ONE_MILLISECOND,

	/** The initial time to wait after waiting with no timeout */
	DEFAULT_WAIT_TIME = 10 * ONE_MICROSECOND,

	/** The minimum time to wait when waiting with a timeout */
	MINIMUM_WAIT_TIME = DEFAULT_WAIT_TIME / 2,

	/** The maximimum time to wait when waiting with a timeout */
	MAXIMUM_WAIT_TIME = ONE_MILLISECOND
};

/**
 * Batch size tuning constants. These are compared to the number of requests
 * that have been processed since the worker thread last woke up.
 **/
enum {
	MINIMUM_BATCH = 32, // wait time increases if batch smaller than this
	MAXIMUM_BATCH = 64  // wait time decreases if batch larger than this
};

struct uds_request_queue {
	const char *name; // name of queue
	uds_request_queue_processor_t *process_one; // function to process 1
						    // request

	struct funnel_queue *main_queue;  // new incoming requests
	struct funnel_queue *retry_queue; // old requests to retry first
	struct event_count *work_event;   // signal to wake the worker thread

	struct thread *thread; // thread id of the worker thread
	bool started;          // true if the worker was started

	bool alive;    // when true, requests can be enqueued

	/** A flag set when the worker is waiting without a timeout */
	atomic_t dormant;

	// The following fields are mutable state private to the worker thread.
	// The first field is aligned to avoid cache line sharing with
	// preceding fields.

	/** requests processed since last wait */
	uint64_t current_batch __attribute__((aligned(CACHE_LINE_BYTES)));

	/** the amount of time to wait to accumulate a batch of requests */
	uint64_t wait_nanoseconds;

	/** the relative time at which to wake when waiting with a timeout */
	ktime_t wake_rel_time;
};

/**
 * Adjust the wait time if the last batch of requests was larger or smaller
 * than the tuning constants.
 *
 * @param queue  the request queue
 **/
static void adjust_wait_time(struct uds_request_queue *queue)
{
	// Adjust the wait time by about 25%.
	uint64_t delta = queue->wait_nanoseconds / 4;

	if (queue->current_batch < MINIMUM_BATCH) {
		// Batch too small, so increase the wait a little.
		queue->wait_nanoseconds += delta;
	} else if (queue->current_batch > MAXIMUM_BATCH) {
		// Batch too large, so decrease the wait a little.
		queue->wait_nanoseconds -= delta;
	}
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
			// The dormant flag was set on the last timeout cycle
			// and nothing changed, so wait with no timeout and
			// reset the wait time.
			queue->wait_nanoseconds = DEFAULT_WAIT_TIME;
			return NULL;
		}
		// Wait one time with the dormant flag set, ensuring that
		// enqueuers will have a chance to see that the flag is set.
		queue->wait_nanoseconds = MAXIMUM_WAIT_TIME;
		atomic_set_release(&queue->dormant, true);
	} else if (queue->wait_nanoseconds < MINIMUM_WAIT_TIME) {
		// If the producer is very fast or the scheduler just doesn't
		// wake us promptly, waiting for very short times won't make
		// the batches smaller.
		queue->wait_nanoseconds = MINIMUM_WAIT_TIME;
	}

	ktime_t *wake = &queue->wake_rel_time;
	*wake = queue->wait_nanoseconds;
	return wake;
}

/**********************************************************************/
static struct uds_request *remove_head(struct funnel_queue *queue)
{
	struct funnel_queue_entry *entry = funnel_queue_poll(queue);
	if (entry == NULL) {
		return NULL;
	}
	return container_of(entry, struct uds_request, request_queue_link);
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
	struct uds_request *request = remove_head(queue->retry_queue);
	if (request == NULL) {
		request = remove_head(queue->main_queue);
	}
	return request;
}

/**
 * Remove the next request to be processed from the queue, waiting for a
 * request if the queue is empty. Must only be called by the worker thread.
 *
 * @param queue  the queue from which to remove an entry
 *
 * @return the next request in the queue, or NULL if the queue has been
 *         shut down and the worker thread should exit
 **/
static struct uds_request *dequeue_request(struct uds_request_queue *queue)
{
	for (;;) {
		// Assume we'll find a request to return; if not, it'll be
		// zeroed later.
		queue->current_batch += 1;

		// Fast path: pull an item off a non-blocking queue and return
		// it.
		struct uds_request *request = poll_queues(queue);
		if (request != NULL) {
			return request;
		}

		// Looks like there's no work. Prepare to wait for more work.
		// If the event count is signalled after this returns, we won't
		// wait later on.
		event_token_t wait_token =
			event_count_prepare(queue->work_event);

		// First poll for shutdown to ensure we don't miss work that
		// was enqueued immediately before a shutdown request.
		bool shutting_down = !READ_ONCE(queue->alive);
		if (shutting_down) {
			/*
			 * Ensure that we see any requests that were guaranteed
			 * to have been fully enqueued before shutdown was
			 * flagged.  The corresponding write barrier is in
			 * uds_request_queue_finish.
			 */
			smp_rmb();
		}

		// Poll again before waiting--a request may have been enqueued
		// just before we got the event key.
		request = poll_queues(queue);
		if ((request != NULL) || shutting_down) {
			event_count_cancel(queue->work_event, wait_token);
			return request;
		}

		// We're about to wait again, so update the wait time to
		// reflect the batch of requests we processed since the last
		// wait.
		adjust_wait_time(queue);

		// If the event count hasn't been signalled since we got the waitToken,
		// wait until it is signalled or until the wait times out.
		ktime_t *wake_time = get_wake_time(queue);
		event_count_wait(queue->work_event, wait_token, wake_time);

		if (wake_time == NULL) {
			// We've been roused from dormancy. Clear the flag so
			// enqueuers can stop broadcasting (no fence needed for
			// this transition).
			atomic_set(&queue->dormant, false);
			// Reset the timeout back to the default since we don't
			// know how long we've been asleep and we also want to
			// be responsive to a new burst.
			queue->wait_nanoseconds = DEFAULT_WAIT_TIME;
		}

		// Just finished waiting, so start counting a new batch.
		queue->current_batch = 0;
	}
}

/**********************************************************************/
static void request_queue_worker(void *arg)
{
	struct uds_request_queue *queue = (struct uds_request_queue *) arg;
	uds_log_debug("%s queue starting", queue->name);
	struct uds_request *request;
	while ((request = dequeue_request(queue)) != NULL) {
		queue->process_one(request);
	}
	uds_log_debug("%s queue done", queue->name);
}

/**********************************************************************/
int make_uds_request_queue(const char *queue_name,
			   uds_request_queue_processor_t *process_one,
			   struct uds_request_queue **queue_ptr)
{
	struct uds_request_queue *queue;
	int result = UDS_ALLOCATE(1, struct uds_request_queue, __func__,
				  &queue);
	if (result != UDS_SUCCESS) {
		return result;
	}
	queue->name = queue_name;
	queue->process_one = process_one;
	queue->alive = true;
	queue->current_batch = 0;
	queue->wait_nanoseconds = DEFAULT_WAIT_TIME;

	result = make_funnel_queue(&queue->main_queue);
	if (result != UDS_SUCCESS) {
		uds_request_queue_finish(queue);
		return result;
	}

	result = make_funnel_queue(&queue->retry_queue);
	if (result != UDS_SUCCESS) {
		uds_request_queue_finish(queue);
		return result;
	}

	result = make_event_count(&queue->work_event);
	if (result != UDS_SUCCESS) {
		uds_request_queue_finish(queue);
		return result;
	}

	result = uds_create_thread(request_queue_worker, queue, queue_name,
				   &queue->thread);
	if (result != UDS_SUCCESS) {
		uds_request_queue_finish(queue);
		return result;
	}

	queue->started = true;
	smp_mb();
	*queue_ptr = queue;
	return UDS_SUCCESS;
}

/**********************************************************************/
static INLINE void wake_up_worker(struct uds_request_queue *queue)
{
	event_count_broadcast(queue->work_event);
}

/**********************************************************************/
void uds_request_queue_enqueue(struct uds_request_queue *queue,
			       struct uds_request *request)
{
	bool unbatched = request->unbatched;
	funnel_queue_put(request->requeued ? queue->retry_queue :
					     queue->main_queue,
			 &request->request_queue_link);

	/*
	 * We must wake the worker thread when it is dormant (waiting with no
	 * timeout). An atomic load (read fence) isn't needed here since we
	 * know the queue operation acts as one.
	 */
	if (atomic_read(&queue->dormant) || unbatched) {
		wake_up_worker(queue);
	}
}

/**********************************************************************/
void uds_request_queue_finish(struct uds_request_queue *queue)
{
	if (queue == NULL) {
		return;
	}

	/*
	 * This memory barrier ensures that any requests we queued will be
	 * seen.  The point is that when dequeue_request sees the following
	 * update to the alive flag, it will also be able to see any change we
	 * made to a next field in the funnel queue entry.  The corresponding
	 * read barrier is in dequeue_request.
	 */
	smp_wmb();

	// Mark the queue as dead.
	WRITE_ONCE(queue->alive, false);

	if (queue->started) {
		// Wake the worker so it notices that it should exit.
		wake_up_worker(queue);

		// Wait for the worker thread to finish processing any
		// additional pending work and exit.
		int result = uds_join_threads(queue->thread);
		if (result != UDS_SUCCESS) {
			uds_log_warning_strerror(result,
						 "Failed to join worker thread");
		}
	}

	free_event_count(queue->work_event);
	free_funnel_queue(queue->main_queue);
	free_funnel_queue(queue->retry_queue);
	UDS_FREE(queue);
}
