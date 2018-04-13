/*
 * Copyright (c) 2018 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/timeDefs.h#1 $
 */

#ifndef LINUX_USER_TIME_DEFS_H
#define LINUX_USER_TIME_DEFS_H

#include <sys/time.h>
#include <time.h>

#include "compiler.h"

typedef enum clockType {
  CT_REALTIME  = CLOCK_REALTIME,
  CT_MONOTONIC = CLOCK_MONOTONIC
} ClockType;

typedef struct timespec AbsTime;

#define ABSTIME_EPOCH { 0, 0 }

#ifdef __cplusplus
extern "C" {
#endif

static INLINE struct timespec asTimeSpec(AbsTime time)
{
  return time;
}

static INLINE struct timeval asTimeVal(AbsTime time)
{
  struct timeval tv = { time.tv_sec, time.tv_nsec / 1000 };
  return tv;
}

static INLINE time_t asTimeT(AbsTime time)
{
  return time.tv_sec;
}

static INLINE AbsTime fromTimeT(time_t time)
{
  AbsTime abs;

  abs.tv_sec = time;
  abs.tv_nsec = 0;
  return abs;
}

#ifdef __cplusplus
}
#endif

#endif // LINUX_USER_TIME_DEFS_H
