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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/minisyslog.c#1 $
 */

#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "logger.h"
#include "memoryAlloc.h"
#include "minisyslog.h"
#include "stringUtils.h"
#include "threads.h"
#include "timeUtils.h"

static Mutex mutex = MUTEX_INITIALIZER;

static int logSocket = -1;

static char *logIdent;

static int logOption;

static int defaultFacility = LOG_USER;

/**********************************************************************/
static void closeLocked(void)
{
  if (logSocket != -1) {
    close(logSocket);
    logSocket = -1;
  }
}

/**********************************************************************/
static void openSocketLocked(void)
{
  if (logSocket != -1) {
    return;
  }

  struct sockaddr_un sun;
  memset(&sun, 0, sizeof(sun));
  sun.sun_family = AF_UNIX;
  strncpy(sun.sun_path, _PATH_LOG, sizeof(sun.sun_path));

  /*
   * We can't log from here, we'll deadlock, so we can't use loggingSocket(),
   * loggingConnect(), or tryCloseFile().
   */
  logSocket = socket(PF_UNIX, SOCK_DGRAM, 0);
  if (logSocket < 0) {
    return;
  }

  if (connect(logSocket, (const struct sockaddr *) &sun, sizeof(sun)) != 0) {
    closeLocked();
  }
}

/**********************************************************************/
void miniOpenlog(const char *ident, int option, int facility)
{
  lockMutex(&mutex);
  closeLocked();
  FREE(logIdent);
  if (duplicateString(ident, NULL, &logIdent) != UDS_SUCCESS) {
    // on failure, NULL is okay
    logIdent = NULL;
  }
  logOption       = option;
  defaultFacility = facility;
  if (logOption & LOG_NDELAY) {
    openSocketLocked();
  }
  unlockMutex(&mutex);
}

/**********************************************************************/
void miniSyslog(int priority, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  miniVsyslog(priority, format, args);
  va_end(args);
}

/**********************************************************************/
__attribute__((warn_unused_result))
static bool writeMsg(int fd, const char *msg)
{
  size_t bytesToWrite = strlen(msg);
  ssize_t bytesWritten = write(fd, msg, bytesToWrite);
  if (bytesWritten == (ssize_t)bytesToWrite) {
    bytesToWrite += 1;
    bytesWritten += write(fd, "\n", 1);
  }
  return (bytesWritten != (ssize_t)bytesToWrite);
}

/**********************************************************************/
__attribute__((format(printf, 3, 0)))
static void logIt(int         priority,
                  const char *prefix,
                  const char *format1,
                  va_list     args1,
                  const char *format2,
                  va_list     args2)
{
  const char *priorityStr = priorityToString(priority);
  char        buffer[1024];
  char       *bufEnd = buffer + sizeof(buffer);
  char       *bufp = buffer;
  time_t      t = asTimeT(currentTime(CT_REALTIME));
  struct tm   tm;
  char        timestamp[64];
  timestamp[0] = '\0';
  if (localtime_r(&t, &tm) != NULL) {
    if (strftime(timestamp, sizeof(timestamp), "%b %e %H:%M:%S", &tm) == 0) {
      timestamp[0] = '\0';
    }
  }
  if (LOG_FAC(priority) == 0) {
    priority |= defaultFacility;
  }

  bufp = appendToBuffer(bufp, bufEnd, "<%d>%s", priority, timestamp);
  const char *stderrMsg = bufp;
  bufp = appendToBuffer(bufp, bufEnd, " %s", logIdent == NULL ? "" : logIdent);

  if (logOption & LOG_PID) {
    char tname[16];
    getThreadName(tname);
    bufp = appendToBuffer(bufp, bufEnd, "[%u]: %-6s (%s/%d) ",
                          getpid(), priorityStr, tname, getThreadId());
  } else {
    bufp = appendToBuffer(bufp, bufEnd, ": ");
  }
  if ((bufp + sizeof("...")) >= bufEnd) {
    return;
  }
  if (prefix != NULL) {
    bufp = appendToBuffer(bufp, bufEnd, "%s", prefix);
  }
  if (format1 != NULL) {
    int ret = vsnprintf(bufp, bufEnd - bufp, format1, args1);
    if (ret < (bufEnd - bufp)) {
      bufp += ret;
    } else {
      bufp = bufEnd;
    }
  }
  if (format2 != NULL) {
    int ret = vsnprintf(bufp, bufEnd - bufp, format2, args2);
    if (ret < (bufEnd - bufp)) {
      bufp += ret;
    } else {
      bufp = bufEnd;
    }
  }
  if (bufp == bufEnd) {
    strcpy(bufEnd - sizeof("..."), "...");
  }
  bool failure = false;
  if (logOption & LOG_PERROR) {
    failure |= writeMsg(STDERR_FILENO, stderrMsg);
  }
  openSocketLocked();
  failure |= (logSocket == -1);
  if (logSocket != -1) {
    size_t bytesToWrite = bufp - buffer;
    ssize_t bytesWritten = send(logSocket, buffer, bytesToWrite, MSG_NOSIGNAL);
    failure |= (bytesWritten != (ssize_t)bytesToWrite);
  }
  if (failure && (logOption & LOG_CONS)) {
    int console = open(_PATH_CONSOLE, O_WRONLY);
    failure |= (console == -1) || writeMsg(console, stderrMsg);
    if (console != -1) {
      failure |= (close(console) != 0);
    }
  }
}

void miniSyslogPack(int         priority,
                    const char *prefix,
                    const char *fmt1,
                    va_list     args1,
                    const char *fmt2,
                    va_list     args2)
{
  lockMutex(&mutex);
  logIt(priority, prefix, fmt1, args1, fmt2, args2);
  unlockMutex(&mutex);
}

void miniVsyslog(int priority, const char *format, va_list ap)
{
  va_list dummy;
  memset(&dummy, 0, sizeof(dummy));
  lockMutex(&mutex);
  logIt(priority, NULL, format, ap, NULL, dummy);
  unlockMutex(&mutex);
}

void miniCloselog(void)
{
  lockMutex(&mutex);
  closeLocked();
  FREE(logIdent);
  logIdent        = NULL;
  logOption       = 0;
  defaultFacility = LOG_USER;
  unlockMutex(&mutex);
}
