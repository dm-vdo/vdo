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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/loggerLinuxUser.c#1 $
 */

#include "logger.h"

#include <execinfo.h>
#include <stdio.h>
#include <unistd.h>

#include "fileUtils.h"
#include "memoryAlloc.h"
#include "stringUtils.h"
#include "threads.h"

const char TIMESTAMPS_ENVIRONMENT_VARIABLE[] = "UDS_LOG_TIMESTAMPS";
const char IDS_ENVIRONMENT_VARIABLE[]        = "UDS_LOG_IDS";

static const char IDENTITY[]       = "UDS";

static OnceState loggerOnce = ONCE_STATE_INITIALIZER;
static Mutex     loggerMutex;   // never destroyed....

static unsigned int  opened      = 0;
static FILE         *fp          = NULL;
static bool          timestamps  = true;
static bool          ids         = true;

/**********************************************************************/
static void initLogger(void)
{
  int result = initMutex(&loggerMutex);
  if (result != UDS_SUCCESS) {
    logErrorWithStringError(result, "cannot initialize logger mutex");
  }
}

/**********************************************************************/
void openLogger(void)
{
  performOnce(&loggerOnce, initLogger);

  lockMutex(&loggerMutex);
  if (opened > 0) {
    ++opened;
    unlockMutex(&loggerMutex);
    return;
  }

  const char *udsLogLevel = getenv("UDS_LOG_LEVEL");
  if (udsLogLevel != NULL) {
    setLogLevel(stringToPriority(udsLogLevel));
  } else {
    setLogLevel(LOG_INFO);
  }

  char *timestampsString = getenv(TIMESTAMPS_ENVIRONMENT_VARIABLE);
  if (timestampsString != NULL && strcmp(timestampsString, "0") == 0) {
    timestamps = false;
  }

  char *idsString = getenv(IDS_ENVIRONMENT_VARIABLE);
  if (idsString != NULL && strcmp(idsString, "0") == 0) {
    ids = false;
  }

  int error = 0;
  char *logFile = getenv("UDS_LOGFILE");
  bool isAbsPath = false;
  if (logFile != NULL) {
    isAbsPath = (makeAbsPath(logFile, &logFile) == UDS_SUCCESS);
    errno = 0;
    fp = fopen(logFile, "a");
    if (fp != NULL) {
      if (isAbsPath) {
        FREE(logFile);
      }
      opened = 1;
      unlockMutex(&loggerMutex);
      return;
    }
    error = errno;
  }

  char *identity;
  if (allocSprintf(NULL, &identity, "%s/%s", IDENTITY,
                   program_invocation_short_name)
      == UDS_SUCCESS) {
    miniOpenlog(identity, LOG_PID | LOG_NDELAY | LOG_CONS, LOG_USER);
    FREE(identity);
  } else {
    miniOpenlog(IDENTITY, LOG_PID | LOG_NDELAY | LOG_CONS, LOG_USER);
    logError("Could not include program name in log");
  }

  if (error != 0) {
    logErrorWithStringError(error, "Couldn't open log file %s", logFile);
  }

  if (isAbsPath) {
    FREE(logFile);
  }
  opened = 1;
  unlockMutex(&loggerMutex);
}

/**********************************************************************/
void closeLogger(void)
{
  performOnce(&loggerOnce, initLogger);

  lockMutex(&loggerMutex);
  if (opened > 0 && --opened == 0) {
    if (fp == NULL) {
      miniCloselog();
    } else {
      fclose(fp);
      fp = NULL;
    }
  }
  unlockMutex(&loggerMutex);
}

/**********************************************************************/
static void formatCurrentTime(char *buffer, size_t bufferSize)
{
  *buffer = 0;

  AbsTime now = currentTime(CT_REALTIME);
  if (!isValidTime(now)) {
    return;
  }

  struct timeval tv = asTimeVal(now);

  struct tm tmp;
  if (localtime_r(&tv.tv_sec, &tmp) == NULL) {
    return;
  }

  if (strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", &tmp) == 0) {
    *buffer = 0;
    return;
  }

  size_t currentLength = strlen(buffer);
  if (currentLength > (bufferSize - 5)) {
    // Not enough room to add milliseconds but we do have a time string.
    return;
  }
  snprintf(buffer + currentLength, bufferSize - currentLength,
           ".%03d", (int) (tv.tv_usec / 1000));
}

/**********************************************************************/
void logMessagePack(int         priority,
                    const char *prefix,
                    const char *fmt1,
                    va_list     args1,
                    const char *fmt2,
                    va_list     args2)
{
  if (priority > getLogLevel()) {
    return;
  }

  // Preserve errno since the caller cares more about their own error state
  // than about errors in the logging code.
  int error = errno;

  if (fp == NULL) {
    miniSyslogPack(priority, prefix, fmt1, args1, fmt2, args2);
  } else {
    char tname[16];
    getThreadName(tname);
    flockfile(fp);

    if (timestamps) {
      char timeBuffer[32];
      formatCurrentTime(timeBuffer, sizeof(timeBuffer));
      fprintf(fp, "%s ", timeBuffer);
    }

    fputs(program_invocation_short_name, fp);

    if (ids) {
      fprintf(fp, "[%u]", getpid());
    }

    fprintf(fp, ": %-6s (%s", priorityToString(priority), tname);

    if (ids) {
      fprintf(fp, "/%d", getThreadId());
    }

    fputs(") ", fp);

    if (prefix != NULL) {
      fputs(prefix, fp);
    }
    if (fmt1 != NULL) {
      vfprintf(fp, fmt1, args1);
    }
    if (fmt2 != NULL) {
      vfprintf(fp, fmt2, args2);
    }
    putc('\n', fp);
    fflush(fp);
    funlockfile(fp);
  }

  // Reset errno
  errno = error;
}

/**********************************************************************/
__attribute__((format(printf, 2, 3)))
static void logAtLevel(int priority, const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vLogMessage(priority, format, args);
  va_end(args);
}

/**
 * Log the contents of /proc/self/maps so that we can decode the addresses
 * in a stack trace.
 *
 * @param priority The priority at which to log
 **/
static void logProcMaps(int priority)
{
  FILE *mapsFile = fopen("/proc/self/maps", "r");
  if (mapsFile == NULL) {
    return;
  }

  logAtLevel(priority, "maps file");
  char buffer[1024];
  char *mapLine;
  while ((mapLine = fgets(buffer, 1024, mapsFile)) != NULL) {
    char *newline = strchr(mapLine, '\n');
    if (newline != NULL) {
      *newline = '\0';
    }
    logAtLevel(priority, "  %s", mapLine);
  }
  logAtLevel(priority, "end of maps file");

  fclose(mapsFile);
}

enum { NUM_STACK_FRAMES = 32 };

/**********************************************************************/
void logBacktrace(int priority)
{
  logAtLevel(priority, "[backtrace]");
  void *trace[NUM_STACK_FRAMES];
  int traceSize = backtrace(trace, NUM_STACK_FRAMES);
  char **messages = backtrace_symbols(trace, traceSize);
  if (messages == NULL) {
    logAtLevel(priority, "backtrace failed");
  } else {
    for (int i = 0; i < traceSize; ++i) {
      logAtLevel(priority, "  %s", messages[i]);
    }
    // "messages" is malloc'ed indirectly by backtrace_symbols
    free(messages);
    logProcMaps(priority);
  }
}

/**********************************************************************/
void pauseForLogger(void)
{
  // User-space logger can't be overrun, so this is a no-op.
}
