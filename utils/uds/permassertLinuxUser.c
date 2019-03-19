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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/permassertLinuxUser.c#1 $
 */

#include "permassert.h"
#include "permassertInternals.h"

#ifdef NDEBUG
#define DEBUGGING_OFF
#undef NDEBUG
#endif /* NDEBUG */

#include <assert.h>
#include <stdlib.h>
#include <syslog.h>

#include "common.h"
#include "errors.h"
#include "logger.h"
#include "stringUtils.h"
#include "threads.h"

#ifdef DEBUGGING_OFF
static bool exitOnAssertionFailure = false;
#else /* !DEBUGGING_OFF */
static bool exitOnAssertionFailure = true;
#endif /* DEBUGGING_OFF */

static const char *EXIT_ON_ASSERTION_FAILURE_VARIABLE
  = "UDS_EXIT_ON_ASSERTION_FAILURE";

static OnceState initOnce = ONCE_STATE_INITIALIZER;
static Mutex     mutex    = MUTEX_INITIALIZER;

typedef void (*assertFailFunc)(const char *  assertion,
                               const char   *file,
                               unsigned int  line,
                               const char   *function);

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
  performOnce(&initOnce, initialize);
  lockMutex(&mutex);
  bool previousSetting = exitOnAssertionFailure;
  exitOnAssertionFailure = shouldExit;
  unlockMutex(&mutex);
  return previousSetting;
}

/**********************************************************************/
__attribute__((format(printf, 4, 0)))
void handleAssertionFailure(const char *expressionString,
                            const char *fileName,
                            int         lineNumber,
                            const char *format,
                            va_list     args)
{
  logEmbeddedMessage(LOG_ERR, "assertion ", format, args,
                     " (%s) failed at %s:%d",
                     expressionString, fileName, lineNumber);
  logBacktrace(LOG_ERR);

  performOnce(&initOnce, initialize);
  lockMutex(&mutex);
  if (exitOnAssertionFailure) {
    __assert_fail(expressionString, fileName, lineNumber, __ASSERT_FUNCTION);
  }
  unlockMutex(&mutex);
}
