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
 * $Id: //eng/uds-releases/krusty/src/uds/timeUtils.h#9 $
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
#define NSEC_PER_SEC  1000000000L
#define NSEC_PER_MSEC 1000000L
#define NSEC_PER_USEC 1000L
typedef int64_t ktime_t;
#endif

/**
 * Return the current time according to the specified clock type.
 *
 * @param clock         Either CLOCK_REALTIME or CLOCK_MONOTONIC
 *
 * @return the current time according to the clock in question
 *
 * @note the precision of the clock is system specific
 **/
#ifdef __KERNEL__
static INLINE ktime_t currentTime(clockid_t clock)
{
  // clock is always a constant, so gcc reduces this to a single call
  return clock == CLOCK_MONOTONIC ? ktime_get_ns() : ktime_get_real_ns();
}
#else
ktime_t currentTime(clockid_t clock);
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
struct timespec futureTime(ktime_t offset);
#endif

#ifndef __KERNEL__
/**
 * Return the difference between two timestamps.
 *
 * @param a  A time
 * @param b  Another time, based on the same clock as a.
 *
 * @return the relative time between the two timestamps
 **/
static INLINE ktime_t ktime_sub(ktime_t a, ktime_t b)
{
  return a - b;
}
#endif /* __KERNEL__ */



/**
 * Convert a ktime_t value to milliseconds
 *
 * @param abstime  The absolute time
 *
 * @return the equivalent number of milliseconds since the epoch
 **/
static INLINE int64_t absTimeToMilliseconds(ktime_t abstime)
{
  return abstime / NSEC_PER_MSEC;
}

/**
 * Convert seconds to a ktime_t value
 *
 * @param seconds  A number of seconds
 *
 * @return the equivalent number of seconds as a ktime_t
 **/
static INLINE ktime_t secondsToRelTime(int64_t seconds)
{
  return (ktime_t) seconds * (1000 * 1000 * 1000);
}

/**
 * Convert milliseconds to a ktime_t value
 *
 * @param milliseconds  A number of milliseconds
 *
 * @return the equivalent number of milliseconds as a ktime_t
 **/
static INLINE ktime_t millisecondsToRelTime(int64_t milliseconds)
{
  return (ktime_t) milliseconds * (1000 * 1000);
}

/**
 * Convert microseconds to a ktime_t value
 *
 * @param microseconds  A number of microseconds
 *
 * @return the equivalent number of microseconds as a ktime_t
 **/
static INLINE ktime_t microsecondsToRelTime(int64_t microseconds)
{
  return (ktime_t) microseconds * 1000;
}

/**
 * Convert a rel_time_t value to milliseconds
 *
 * @param reltime  The relative time
 *
 * @return the equivalent number of milliseconds
 **/
static INLINE int64_t relTimeToSeconds(ktime_t reltime)
{
  return reltime / (1000 * 1000 * 1000);
}

/**
 * Convert a ktime_t value to milliseconds
 *
 * @param reltime  The relative time
 *
 * @return the equivalent number of milliseconds
 **/
static INLINE int64_t relTimeToMilliseconds(ktime_t reltime)
{
  return reltime / (1000 * 1000);
}

/**
 * Convert a ktime_t value to microseconds
 *
 * @param reltime  The relative time
 *
 * @return the equivalent number of microseconds
 **/
static INLINE int64_t relTimeToMicroseconds(ktime_t reltime)
{
  return reltime / 1000;
}

/**
 * Return the wall clock time in microseconds. The actual value is time
 * since the epoch (see "man gettimeofday"), but the typical use is to call
 * this twice and compute the difference, giving the elapsed time between
 * the two calls.
 *
 * @return the time in microseconds
 **/
uint64_t __must_check nowUsec(void);

/**
 * Convert from a ktime_t to seconds truncating
 *
 * @param time  a ktime_t time
 *
 * @return a 64 bit signed number of seconds
 **/
static INLINE int64_t absTimeToSeconds(ktime_t time)
{
  return time / NSEC_PER_SEC;
}

/**
 * Convert from seconds to a ktime_t,
 *
 * @param time  a 64 bit signed number of seconds
 *
 * @return a ktime_t time
 **/
static INLINE ktime_t fromSeconds(int64_t time)
{
  return time * NSEC_PER_SEC;
}

#ifndef __KERNEL__
/**
 * Convert from a ktime_t to a time_t
 *
 * @param time  a ktime_t time
 *
 * @return a time_t time
 **/
static INLINE time_t asTimeT(ktime_t time)
{
  return time / NSEC_PER_SEC;
}

#endif /* __KERNEL__ */

#endif /* TIME_UTILS_H */
