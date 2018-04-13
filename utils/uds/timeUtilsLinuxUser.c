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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/timeUtilsLinuxUser.c#1 $
 */

#include "timeUtils.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

#include "errors.h"
#include "stringUtils.h"

static const struct timespec invalidTime = {
  .tv_sec  = -1,
  .tv_nsec = LONG_MAX
};

static const long BILLION = 1000 * 1000 * 1000;

/*****************************************************************************/
bool isValidTime(AbsTime time)
{
  if (time.tv_nsec < 0 || time.tv_nsec >= BILLION) {
    return false;
  }
  return true;
}

/*****************************************************************************/
AbsTime currentTime(ClockType clock)
{
  struct timespec ts;
  if (clock_gettime(clock, &ts) != 0) {
    ts = invalidTime;
  }
  return ts;
}

/*****************************************************************************/
AbsTime futureTime(ClockType clock, RelTime reltime)
{
  return deltaTime(currentTime(clock), reltime);
}

/*****************************************************************************/
AbsTime deltaTime(AbsTime time, RelTime reltime)
{
  if (!isValidTime(time)) {
    return time;
  }
  if ((reltime >= 0) && (reltime < 10 * BILLION)) {
    reltime += time.tv_nsec;
    while (reltime >= BILLION) {
      reltime -= BILLION;
      time.tv_sec++;
    }
    time.tv_nsec = reltime;
    return time;
  }
  // may not be accurate for times before the Epoch...
  // (is the ns time positive or negative for negative time_t?)
  int64_t ns = time.tv_sec * BILLION + time.tv_nsec;
  if ((ns < INT64_MIN / 2) ||
      (ns > INT64_MAX / 2) ||
      (reltime < INT64_MIN / 2) ||
      (reltime > INT64_MAX / 2)) {
    return invalidTime;
  }
  ns += reltime;
  return (AbsTime) { .tv_sec = ns / BILLION, .tv_nsec = ns % BILLION };
}

/*****************************************************************************/
RelTime timeDifference(AbsTime a, AbsTime b)
{
  if (isValidTime(a) && isValidTime(b)) {
    int64_t ans = a.tv_sec * BILLION + a.tv_nsec;
    int64_t bns = b.tv_sec * BILLION + b.tv_nsec;
    return ans - bns;
  } else if (isValidTime(a)) {
    return INT64_MAX;
  } else if (isValidTime(b)) {
    return INT64_MIN;
  } else {
    return 0;
  }
}

/*****************************************************************************/
void sleepFor(RelTime reltime)
{
  int ret;
  struct timespec duration, remaining;
  if (reltime < 0) {
    return;
  }
  remaining.tv_sec  = reltime / BILLION;
  remaining.tv_nsec = reltime % BILLION;
  do {
    duration = remaining;
    ret = nanosleep(&duration, &remaining);
  } while ((ret == -1) && (errno == EINTR));
}

/*****************************************************************************/
static long roundToSubseconds(long nsec, unsigned int subseconds)
{
  int exp = (subseconds > 9) ? 0 : 9 - subseconds;
  long div = 1;
  while (exp > 0) {
    div *= 10;
    --exp;
  }
  return nsec / div;
}

/*****************************************************************************/
int timeInISOFormat(AbsTime       time,
                    char         *buf,
                    size_t        bufSize,
                    unsigned int  subseconds)
{
  struct tm tmbuf;

  struct tm *tm = localtime_r(&time.tv_sec, &tmbuf);

  char *bp = buf;
  char *be = buf + bufSize;
  if (!tm) {
    if (subseconds) {
      bp = appendToBuffer(bp, be, "[%ld.%0*ld]", (long) time.tv_sec, subseconds,
                          roundToSubseconds(time.tv_nsec, subseconds));
    } else {
      bp = appendToBuffer(bp, be, "[%ld]", (long) time.tv_sec);
    }
  } else {
    size_t n = strftime(bp, be - bp, "%Y-%m-%d %H:%M:%S", tm);
    bp += n;
    if (subseconds) {
      bp = appendToBuffer(bp, be, ".%0*ld", subseconds,
                          roundToSubseconds(time.tv_nsec, subseconds));
    }
  }
  return (bp < be) ? UDS_SUCCESS : UDS_BUFFER_ERROR;
}
