// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include "time-utils.h"

ktime_t current_time_ns(clockid_t clock)
{
	struct timespec ts;

	if (clock_gettime(clock, &ts) != 0)
		ts = (struct timespec) { 0, 0 };
	return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

struct timespec future_time(ktime_t offset)
{
	ktime_t future = current_time_ns(CLOCK_REALTIME) + offset;

	return (struct timespec) {
		.tv_sec = future / NSEC_PER_SEC,
		.tv_nsec = future % NSEC_PER_SEC,
	};
}

int64_t current_time_us(void)
{
	return current_time_ns(CLOCK_REALTIME) / NSEC_PER_USEC;
}
