/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef UDS_TIME_UTILS_H
#define UDS_TIME_UTILS_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <sys/time.h>
#include <time.h>

/* Some constants that are defined in kernel headers. */
#define NSEC_PER_SEC 1000000000L
#define NSEC_PER_MSEC 1000000L
#define NSEC_PER_USEC 1000L

typedef s64 ktime_t;

static inline s64 ktime_to_seconds(ktime_t reltime)
{
	return reltime / NSEC_PER_SEC;
}

ktime_t __must_check current_time_ns(clockid_t clock);

ktime_t __must_check current_time_us(void);

/* Return a timespec for the current time plus an offset. */
struct timespec future_time(ktime_t offset);

static inline ktime_t ktime_sub(ktime_t a, ktime_t b)
{
	return a - b;
}

static inline s64 ktime_to_ms(ktime_t abstime)
{
	return abstime / NSEC_PER_MSEC;
}

static inline ktime_t ms_to_ktime(u64 milliseconds)
{
	return (ktime_t) milliseconds * NSEC_PER_MSEC;
}

static inline s64 ktime_to_us(ktime_t reltime)
{
	return reltime / NSEC_PER_USEC;
}

#endif /* UDS_TIME_UTILS_H */
