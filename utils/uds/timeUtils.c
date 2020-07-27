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
 * $Id: //eng/uds-releases/krusty/src/uds/timeUtils.c#7 $
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
  return fromTimeSpec(ts);
}
#endif

#ifndef __KERNEL__
/*****************************************************************************/
ktime_t futureTime(clockid_t clock, ktime_t reltime)
{
  return currentTime(clock) + reltime;
}
#endif

/*****************************************************************************/
uint64_t nowUsec(void)
{
  return currentTime(CLOCK_REALTIME) / NSEC_PER_USEC;
}



