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
 * $Id: //eng/uds-releases/krusty/src/uds/timeUtils.c#8 $
 */

#include "permassert.h"
#include "stringUtils.h"
#include "timeUtils.h"

#ifdef __KERNEL__
#include <linux/delay.h>
#include <linux/ktime.h> // for getnstimeofday on Vivid
#else
#include <errno.h>
#endif

#ifndef __KERNEL__
/*****************************************************************************/
ktime_t currentTime(clockid_t clock)
{
  struct timespec ts;
  if (clock_gettime(clock, &ts) != 0) {
    ts = (struct timespec) { 0, 0 };
  }
  return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

/*****************************************************************************/
struct timespec futureTime(ktime_t offset)
{
  ktime_t future = currentTime(CLOCK_REALTIME) + offset;
  return (struct timespec) {
    .tv_sec = future / NSEC_PER_SEC,
    .tv_nsec = future % NSEC_PER_SEC,
  };
}
#endif /* __KERNEL__ */

/*****************************************************************************/
uint64_t nowUsec(void)
{
  return currentTime(CLOCK_REALTIME) / NSEC_PER_USEC;
}



