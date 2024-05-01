/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef EVENT_COUNT_H
#define EVENT_COUNT_H

#include "time-utils.h"

/*
 * An event count is a lock-free equivalent of a condition variable.
 *
 * Using an event count, a lock-free producer/consumer can wait for a state change (adding an item
 * to an empty queue, for example) without spinning or falling back on the use of mutex-based
 * locks. Signalling is cheap when there are no waiters (a memory fence), and preparing to wait is
 * also inexpensive (an atomic add instruction).
 *
 * A lock-free producer should call event_count_broadcast() after any mutation to the lock-free
 * data structure that a consumer might be waiting on. The consumers should poll for work like
 * this:
 *
 *   for (;;) {
 *       // Fast path--no additional cost to consumer.
 *       if (lockfree_dequeue(&item))
 *           return item;
 *       // Two-step wait: get current token and poll state, either cancelling
 *       // the wait or waiting for the token to be signalled.
 *       event_token_t token = event_count_prepare(event_count);
 *       if (lockfree_dequeue(&item)) {
 *           event_count_cancel(event_count, token);
 *           return item;
 *       }
 *       event_count_wait(event_count, token, NULL);
 *       // State has changed, but must check condition again, so loop.
 *   }
 *
 * Once event_count_prepare() is called, the caller should neither sleep nor perform long-running
 * or blocking actions before passing the token to event_count_cancel() or event_count_wait(). The
 * implementation is optimized for a short polling window, and will not perform well if there are
 * outstanding tokens that have been signalled but not waited upon.
 */

struct event_count;

typedef unsigned int event_token_t;

int __must_check make_event_count(struct event_count **count_ptr);

void free_event_count(struct event_count *count);

void event_count_broadcast(struct event_count *count);

event_token_t __must_check event_count_prepare(struct event_count *count);

void event_count_cancel(struct event_count *count, event_token_t token);

bool event_count_wait(struct event_count *count, event_token_t token,
		      const ktime_t *timeout);

#endif /* EVENT_COUNT_H */
