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
 * $Id: //eng/uds-releases/flanders/userLinux/uds/threadMutexLinuxUser.c#3 $
 */

#include <errno.h>
#include <stdlib.h>

#include "permassert.h"
#include "stringUtils.h"
#include "threadOnce.h"
#include "threads.h"

static enum MutexKind {
  FastAdaptive,
  ErrorChecking
} hiddenMutexKind = ErrorChecking;

const bool DO_ASSERTIONS = true;

/**********************************************************************/
static int initMutexAttr(pthread_mutexattr_t *mutexattr)
{
  int result = pthread_mutexattr_init(mutexattr);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "mutexattr: 0x%p", (void *) mutexattr);
}

/**********************************************************************/
static int destroyMutexAttr(pthread_mutexattr_t *mutexattr)
{
  int result = pthread_mutexattr_destroy(mutexattr);
  return ASSERT_WITH_ERROR_CODE((result == 0), result,
                                "mutexattr: 0x%p", (void *) mutexattr);
}

/**********************************************************************/
static void initializeMutexKind(void)
{
  static const char UDS_MUTEX_KIND_ENV[] = "UDS_MUTEX_KIND";

  // Enabling error checking on mutexes enables a great performance loss, so
  // we only enable it in certain circumstances.
#ifdef NDEBUG
  hiddenMutexKind = FastAdaptive;
#endif

  const char *mutexKindString = getenv(UDS_MUTEX_KIND_ENV);
  if (mutexKindString != NULL) {
    if (strcmp(mutexKindString, "error-checking") == 0) {
      hiddenMutexKind = ErrorChecking;
    } else if (strcmp(mutexKindString, "fast-adaptive") == 0) {
      hiddenMutexKind = FastAdaptive;
    } else {
      ASSERT_LOG_ONLY(false,
                      "environment variable %s had unexpected value '%s'",
                      UDS_MUTEX_KIND_ENV, mutexKindString);
    }
  }
}

/**********************************************************************/
static enum MutexKind getMutexKind(void)
{
  static OnceState onceState = ONCE_STATE_INITIALIZER;

  performOnce(&onceState, initializeMutexKind);

  return hiddenMutexKind;
}

/**********************************************************************/
int initializeMutex(Mutex *mutex, bool assertOnError)
{
  pthread_mutexattr_t attr;
  initMutexAttr(&attr);

  if (getMutexKind() == ErrorChecking) {
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
  }
  int result = pthread_mutex_init(mutex, &attr);
  if ((result != 0) && assertOnError) {
    result = ASSERT_WITH_ERROR_CODE((result == 0), result, "mutex: 0x%p",
                                    (void *) mutex);
  }

  int result2 = destroyMutexAttr(&attr);
  if (result == UDS_SUCCESS) {
    result = result2;
  }

  return result;
}

/**********************************************************************/
int initMutex(Mutex *mutex)
{
  return initializeMutex(mutex, DO_ASSERTIONS);
}

/**********************************************************************/
int destroyMutex(Mutex *mutex)
{
  int result = pthread_mutex_destroy(mutex);
  return ASSERT_WITH_ERROR_CODE((result == 0), result, "mutex: 0x%p",
                                (void *) mutex);
}

/**********************************************************************
 * Error checking for mutex calls is only enabled when NDEBUG is not
 * defined. So only check for and report errors when the mutex calls can
 * return errors.
 */

/**********************************************************************/
void lockMutex(Mutex *mutex)
{
  int result __attribute__((unused)) = pthread_mutex_lock(mutex);
#ifndef NDEBUG
  ASSERT_LOG_ONLY((result == 0), "mutex: 0x%p, result: %d",
                  (void *) mutex, result);
#endif
}

/**********************************************************************/
void unlockMutex(Mutex *mutex)
{
  int result  __attribute__((unused)) = pthread_mutex_unlock(mutex);
#ifndef NDEBUG
  ASSERT_LOG_ONLY((result == 0), "mutex: 0x%p, result: %d",
                  (void *) mutex, result);
#endif
}
