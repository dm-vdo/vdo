/*
 * Copyright (c) 2019 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/jasper/userLinux/uds/threadSemaphoreLinuxUser.c#3 $
 */

#include <errno.h>

#include "logger.h"
#include "permassert.h"
#include "threads.h"
#include "timeUtils.h"

/**********************************************************************/
int initializeSemaphore(Semaphore *semaphore, unsigned int value)
{
  int result = sem_init(semaphore, false, value);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "sem_init error");
}

/**********************************************************************/
int destroySemaphore(Semaphore *semaphore)
{
  int result = sem_destroy(semaphore);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "sem_destroy error");
}

/**********************************************************************/
void acquireSemaphore(Semaphore  *semaphore)
{
  int result;
  do {
    result = sem_wait(semaphore);
  } while ((result == -1) && (errno == EINTR));

#ifndef NDEBUG
  ASSERT_LOG_ONLY((result == 0), "sem_wait error %d", errno);
#endif
}

/**********************************************************************/
bool attemptSemaphore(Semaphore *semaphore, RelTime timeout)
{
  if (timeout > 0) {
    struct timespec ts = asTimeSpec(futureTime(CLOCK_REALTIME, timeout));
    do {
      if (sem_timedwait(semaphore, &ts) == 0) {
        return true;
      }
    } while (errno == EINTR);
#ifndef NDEBUG
    ASSERT_LOG_ONLY((errno == ETIMEDOUT), "sem_timedwait error %d", errno);
#endif
  } else {
    do {
      if (sem_trywait(semaphore) == 0) {
        return true;
      }
    } while (errno == EINTR);
#ifndef NDEBUG
    ASSERT_LOG_ONLY((errno == EAGAIN), "sem_trywait error %d", errno);
#endif
  }
  return false;
}

/**********************************************************************/
void releaseSemaphore(Semaphore  *semaphore)
{
  int result __attribute__((unused)) = sem_post(semaphore);
#ifndef NDEBUG
  ASSERT_LOG_ONLY((result == 0), "sem_post error %d", errno);
#endif
}
