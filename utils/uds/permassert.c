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
 * $Id: //eng/uds-releases/krusty/src/uds/permassert.c#2 $
 */

#include "permassert.h"
#include "permassertInternals.h"

#include "errors.h"
#include "logger.h"

#ifndef __KERNEL__
// Here begins a large block of userspace-only stuff.
#ifdef NDEBUG
#define DEBUGGING_OFF
#undef NDEBUG
#endif /* NDEBUG */

#include <assert.h>
#include <stdlib.h>
#include <syslog.h>

#include "common.h"
#include "stringUtils.h"
#include "threads.h"

#ifdef DEBUGGING_OFF
static bool exitOnAssertionFailure = false;
#else /* !DEBUGGING_OFF */
static bool exitOnAssertionFailure = true;
#endif /* DEBUGGING_OFF */

static const char *EXIT_ON_ASSERTION_FAILURE_VARIABLE
  = "UDS_EXIT_ON_ASSERTION_FAILURE";

static once_state_t initOnce = ONCE_STATE_INITIALIZER;
static struct mutex mutex    = { .mutex = MUTEX_INITIALIZER };

/**********************************************************************/
static void initialize(void)
{
  initializeMutex(&mutex, !DO_ASSERTIONS);
  char *exitOnAssertionFailureString
    = getenv(EXIT_ON_ASSERTION_FAILURE_VARIABLE);
  if (exitOnAssertionFailureString != NULL) {
    exitOnAssertionFailure
      = (strcasecmp(exitOnAssertionFailureString, "true") == 0);
  }
}

/**********************************************************************/
bool setExitOnAssertionFailure(bool shouldExit)
{
  perform_once(&initOnce, initialize);
  lockMutex(&mutex);
  bool previousSetting = exitOnAssertionFailure;
  exitOnAssertionFailure = shouldExit;
  unlockMutex(&mutex);
  return previousSetting;
}

// Here ends large block of userspace stuff.
#endif // !__KERNEL__

/**********************************************************************/
__attribute__((format(printf, 4, 0)))
void handleAssertionFailure(const char *expressionString,
                            const char *fileName,
                            int         lineNumber,
                            const char *format,
                            va_list     args)
{
  log_embedded_message(LOG_ERR, "assertion \"", format, args,
                       "\" (%s) failed at %s:%d",
                       expressionString, fileName, lineNumber);
  log_backtrace(LOG_ERR);

#ifndef __KERNEL__
  perform_once(&initOnce, initialize);
  lockMutex(&mutex);
  if (exitOnAssertionFailure) {
    __assert_fail(expressionString, fileName, lineNumber, __ASSERT_FUNCTION);
  }
  unlockMutex(&mutex);
#endif // !__KERNEL__
}

/*****************************************************************************/
int assertionFailed(const char *expressionString,
                    int         code,
                    const char *fileName,
                    int         lineNumber,
                    const char *format,
                    ...)
{
  va_list args;
  va_start(args, format);
  handleAssertionFailure(expressionString, fileName, lineNumber, format, args);
  va_end(args);

  return code;
}

/*****************************************************************************/
int assertionFailedLogOnly(const char *expressionString,
                           const char *fileName,
                           int         lineNumber,
                           const char *format,
                           ...)
{
  va_list args;
  va_start(args, format);
  handleAssertionFailure(expressionString, fileName, lineNumber, format, args);
  va_end(args);

  return UDS_ASSERTION_FAILED;
}
