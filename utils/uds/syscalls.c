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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/syscalls.c#1 $
 */

#include "syscalls.h"

#include <sched.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>

#include "fileUtils.h"
#include "memoryAlloc.h"
#include "permassert.h"

/**********************************************************************/
int loggingReadInterruptible(int         fd,
                             void       *buf,
                             size_t      count,
                             const char *context,
                             ssize_t    *bytesReadPtr)
{
  return checkIOErrors(read(fd, buf, count), __func__, context, bytesReadPtr);
}

/**********************************************************************/
int loggingRead(int         fd,
                void       *buf,
                size_t      count,
                const char *context,
                ssize_t    *bytesReadPtr)
{
  int result;
  do {
    result =  loggingReadInterruptible(fd, buf, count, context, bytesReadPtr);
  } while (result == EINTR);

  return result;
}

/**********************************************************************/
static int loggingPreadInterruptible(int         fd,
                                     void       *buf,
                                     size_t      count,
                                     off_t       offset,
                                     const char *context,
                                     ssize_t    *bytesReadPtr)
{
  return checkIOErrors(pread(fd, buf, count, offset), __func__, context,
                       bytesReadPtr);
}

/**********************************************************************/
int loggingPread(int         fd,
                 void       *buf,
                 size_t      count,
                 off_t       offset,
                 const char *context,
                 ssize_t    *bytesReadPtr)
{
  int result;
  do {
    result = loggingPreadInterruptible(fd, buf, count, offset, context,
                                       bytesReadPtr);
  } while (result == EINTR);

  return result;
}

/**********************************************************************/
int loggingWriteInterruptible(int         fd,
                              const void *buf,
                              size_t      count,
                              const char *context,
                              ssize_t    *bytesWrittenPtr)
{
  return checkIOErrors(write(fd, buf, count), __func__, context,
                       bytesWrittenPtr);
}

/**********************************************************************/
int loggingWrite(int         fd,
                 const void *buf,
                 size_t      count,
                 const char *context,
                 ssize_t    *bytesWrittenPtr)
{
  int result;
  do {
    result = loggingWriteInterruptible(fd, buf, count, context,
                                       bytesWrittenPtr);
  } while (result == EINTR);

  return result;
}

/**********************************************************************/
static int loggingPwriteInterruptible(int         fd,
                                      const void *buf,
                                      size_t      count,
                                      off_t       offset,
                                      const char *context,
                                      ssize_t    *bytesWrittenPtr)
{
  return checkIOErrors(pwrite(fd, buf, count, offset), __func__, context,
                       bytesWrittenPtr);
}

/**********************************************************************/
int loggingPwrite(int         fd,
                  const void *buf,
                  size_t      count,
                  off_t       offset,
                  const char *context,
                  ssize_t    *bytesWrittenPtr)
{
  int result;
  do {
    result = loggingPwriteInterruptible(fd, buf, count, offset, context,
                                        bytesWrittenPtr);
  } while (result == EINTR);

  return result;
}

/**********************************************************************/
int loggingClose(int fd, const char *context)
{
  return checkSystemCall(close(fd), __func__, context);
}

/**********************************************************************/
int processControl(int option, unsigned long arg2, unsigned long arg3,
                   unsigned long arg4, unsigned long arg5)
{
  int result = prctl(option, arg2, arg3, arg4, arg5);
  return ASSERT_WITH_ERROR_CODE(result >= 0, errno,
                                "option: %d, arg2: %lu, arg3: %lu, "
                                "arg4: %lu, arg5: %lu",
                                option, arg2, arg3, arg4, arg5);
}

/**********************************************************************/
int setUpSigmask(const int *signals, sigset_t *sigset, const char *context)
{
  int result = checkSystemCall(sigemptyset(sigset), __func__, context);
  if (result != UDS_SUCCESS) {
    return result;
  }

  if (signals != NULL) {
    for (int i = 0; signals[i] != -1; i++) {
      result = checkSystemCall(sigaddset(sigset, signals[i]),
                               __func__, context);
      if (result != UDS_SUCCESS) {
        return result;
      }
    }
  }

  return UDS_SUCCESS;
}

/**********************************************************************/
int setSignalHandler(int signal, struct sigaction *action, const char *context)
{
  return checkSystemCall(sigaction(signal, action, NULL), __func__, context);
}

/**********************************************************************/
int ignoreSignals(const int *signals, const char *context)
{
  struct sigaction ignore;
  memset(&ignore, 0, sizeof(ignore));
  ignore.sa_handler = SIG_IGN;

  for (int i = 0; signals[i] != -1; i++) {
    int result = setSignalHandler(signals[i], &ignore, context);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  return UDS_SUCCESS;
}

/**********************************************************************/
int restoreSignalHandlers(const int *signals, const char *context)
{
  struct sigaction defaults;
  memset(&defaults, 0, sizeof(defaults));
  defaults.sa_handler = SIG_DFL;

  for (int i = 0; signals[i] != -1; i++) {
    int result = setSignalHandler(signals[i], &defaults, context);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  return UDS_SUCCESS;
}

/**********************************************************************/
int setSignalMask(sigset_t   *mask,
                  int         how,
                  const char *context,
                  sigset_t   *oldMask)
{
  return checkSystemCall(sigprocmask(how, mask, oldMask), __func__, context);
}

/**********************************************************************/
int setThreadSignalMask(sigset_t   *mask,
                        int         how,
                        const char *context,
                        sigset_t   *oldMask)
{
  return checkSystemCall(pthread_sigmask(how, mask, oldMask),
                         __func__, context);
}

/**********************************************************************/
int daemonize(const char *runDir, const char *pidFile, const char *context)
{
  bool nochdir = false;
  int result = makeDirectory(runDir, 0755, "server run dir", __func__);
  if ((result == UDS_SUCCESS) || (result == EEXIST)) {
    result = checkSystemCall(chdir(runDir), __func__, context);
    nochdir = (result == UDS_SUCCESS);
  }
  if (!nochdir) {
    logWarning("failed to change runDir to %s; using '/'.", runDir);
  }

  int fd;
  result = openFile(pidFile, FU_CREATE_READ_WRITE, &fd);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = checkSystemCall(daemon(nochdir, false), __func__, context);
  if (result != UDS_SUCCESS) {
    ASSERT_LOG_ONLY((loggingClose(fd, context) == UDS_SUCCESS),
                    "closing pidFile");
    return result;
  }

  char *strBuf;
  result = allocSprintf(__func__, &strBuf, "%d\n", getpid());
  if (result != UDS_SUCCESS) {
    ASSERT_LOG_ONLY((loggingClose(fd, context) == UDS_SUCCESS),
                    "closing pidFile");
    return logErrorWithStringError(result, "couldn't get pid");
  }
  result = writeBuffer(fd, strBuf, strlen(strBuf));
  FREE(strBuf);
  if (result != UDS_SUCCESS) {
    ASSERT_LOG_ONLY((loggingClose(fd, context) == UDS_SUCCESS),
                    "closing pidFile");
    return result;
  }
  return loggingClose(fd, context);
}
