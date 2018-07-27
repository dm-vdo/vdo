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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/threadSemaphoreLinuxUser.c#2 $
 */

#include "threadSemaphore.h"

#include <errno.h>

#include "logger.h"
#include "permassert.h"
#include "timeUtils.h"

/**********************************************************************/
int initializeSemaphore(Semaphore    *semaphore,
                        unsigned int  value,
                        const char   *context)
{
  int result = sem_init(semaphore, false, value);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "%s: sem_init error",
                                context);
}

/**********************************************************************/
int destroySemaphore(Semaphore *semaphore, const char *context)
{
  int result = sem_destroy(semaphore);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "%s: sem_destroy error",
                                context);
}

/**********************************************************************/
void acquireSemaphore(Semaphore  *semaphore,
                      const char *context __attribute__((unused)))
{
  int result;
  do {
    result = sem_wait(semaphore);
  } while ((result == -1) && (errno == EINTR));

#ifndef NDEBUG
  ASSERT_LOG_ONLY((result == 0), "%s: sem_wait error %d", context, errno);
#endif
}

/**********************************************************************/
bool attemptSemaphore(Semaphore *semaphore,
                      RelTime    timeout,
                      const char *context __attribute__((unused)))
{
  if (timeout > 0) {
    struct timespec ts = asTimeSpec(futureTime(CT_REALTIME, timeout));
    do {
      if (sem_timedwait(semaphore, &ts) == 0) {
        return true;
      }
    } while (errno == EINTR);
#ifndef NDEBUG
    ASSERT_LOG_ONLY((errno == ETIMEDOUT), "%s: sem_timedwait error %d",
                    context, errno);
#endif
  } else {
    do {
      if (sem_trywait(semaphore) == 0) {
        return true;
      }
    } while (errno == EINTR);
#ifndef NDEBUG
    ASSERT_LOG_ONLY((errno == EAGAIN), "%s: sem_trywait error %d",
                    context, errno);
#endif
  }
  return false;
}

/**********************************************************************/
void releaseSemaphore(Semaphore  *semaphore,
                      const char *context __attribute__((unused)))
{
  int result __attribute__((unused)) = sem_post(semaphore);
#ifndef NDEBUG
  ASSERT_LOG_ONLY((result == 0), "%s: sem_post error %d", context, errno);
#endif
}
