/*
 * Copyright (c) 2020 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/krusty/src/uds/timeUtils.h#13 $
 */

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include "compiler.h"
#include "typeDefs.h"

#ifdef __KERNEL__
#include <linux/ktime.h>
#include <linux/time.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

// Some constants that are defined in kernel headers.
#ifndef __KERNEL__
#define NSEC_PER_SEC 1000000000L
#define NSEC_PER_MSEC 1000000L
#define NSEC_PER_USEC 1000L
typedef int64_t ktime_t;
#endif

/**
 * Return the current nanosecond time according to the specified clock
 * type.
 *
 * @param clock         Either CLOCK_REALTIME or CLOCK_MONOTONIC
 *
 * @return the current time according to the clock in question
 **/
#ifdef __KERNEL__
static INLINE ktime_t current_time_ns(clockid_t clock)
{
	// clock is always a constant, so gcc reduces this to a single call
	return clock == CLOCK_MONOTONIC ? ktime_get_ns() : ktime_get_real_ns();
}
#else
ktime_t current_time_ns(clockid_t clock);
#endif

#ifndef __KERNEL__
/**
 * Return a timespec representing a time in the future for timeouts
 *
 * @param offset Nanosecond offset to be added to the current
 *               CLOCK_REALTIME time to compute a future time
 *
 * @return a timespec representing a future time
 **/
struct timespec future_time(ktime_t offset);
#endif /* __KERNEL__ */

#ifndef __KERNEL__
/**
 * Return the difference between two times, as in ktime.h
 *
 * @param a  A time
 * @param b  Another time, based on the same clock as a
 *
 * @return the difference between times a and b
 **/
static INLINE ktime_t ktime_sub(ktime_t a, ktime_t b)
{
	return a - b;
}
#endif /* __KERNEL__ */



#ifndef __KERNEL__
/**
 * Convert a ktime_t value to milliseconds as in ktime.h
 *
 * @param abstime  The absolute time
 *
 * @return the equivalent number of milliseconds since the epoch
 **/
static INLINE int64_t ktime_to_ms(ktime_t abstime)
{
	return abstime / NSEC_PER_MSEC;
}
#endif /* __KERNEL__ */

/**
 * Convert seconds to a ktime_t value
 *
 * @param seconds  A number of seconds
 *
 * @return the equivalent number of seconds as a ktime_t
 **/
static INLINE ktime_t seconds_to_ktime(int64_t seconds)
{
	return (ktime_t) seconds * NSEC_PER_SEC;
}

#ifndef __KERNEL__
/**
 * Convert milliseconds to a ktime_t value as in ktime.h
 *
 * @param milliseconds  A number of milliseconds
 *
 * @return the equivalent number of milliseconds as a ktime_t
 **/
static INLINE ktime_t ms_to_ktime(uint64_t milliseconds)
{
	return (ktime_t) milliseconds * NSEC_PER_MSEC;
}
#endif /* __KERNEL__ */

/**
 * Convert microseconds to a ktime_t value
 *
 * @param microseconds  A number of microseconds
 *
 * @return the equivalent number of microseconds as a ktime_t
 **/
static INLINE ktime_t us_to_ktime(int64_t microseconds)
{
	return (ktime_t) microseconds * NSEC_PER_USEC;
}

/**
 * Convert a ktime_t value to seconds
 *
 * @param reltime  The time value
 *
 * @return the equivalent number of seconds, truncated
 **/
static INLINE int64_t ktime_to_seconds(ktime_t reltime)
{
	return reltime / NSEC_PER_SEC;
}

#ifndef __KERNEL__
/**
 * Convert a ktime_t value to microseconds as in ktime.h
 *
 * @param reltime  The time value
 *
 * @return the equivalent number of microseconds
 **/
static INLINE int64_t ktime_to_us(ktime_t reltime)
{
	return reltime / NSEC_PER_USEC;
}
#endif /* __KERNEL__ */

/**
 * Return the wall clock time in microseconds. The actual value is time
 * since the epoch (see "man gettimeofday"), but the typical use is to call
 * this twice and compute the difference, giving the elapsed time between
 * the two calls.
 *
 * @return the time in microseconds
 **/
int64_t __must_check current_time_us(void);

#ifndef __KERNEL__
#endif /* __KERNEL__ */

#endif /* TIME_UTILS_H */
