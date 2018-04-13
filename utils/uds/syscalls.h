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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/syscalls.h#1 $
 */

#ifndef SYSCALLS_H
#define SYSCALLS_H 1

#include <errno.h>
#include <signal.h>

#include "compiler.h"
#include "logger.h"
#include "typeDefs.h"
#include "uds-error.h"

/**
 * Wrap the read(2) system call.
 *
 * @param fd           The descriptor from which to read
 * @param buf          The buffer to read into
 * @param count        The maximum number of bytes to read
 * @param context      The calling context (for logging)
 * @param bytesReadPtr A pointer to hold the number of bytes read
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingReadInterruptible(int         fd,
                             void       *buf,
                             size_t      count,
                             const char *context,
                             ssize_t    *bytesReadPtr)
  __attribute__((warn_unused_result));

/**
 * Wrap the read(2) system call, looping as long as errno is EINTR.
 *
 * @param fd           The descriptor from which to read
 * @param buf          The buffer to read into
 * @param count        The maximum number of bytes to read
 * @param context      The calling context (for logging)
 * @param bytesReadPtr A pointer to hold the number of bytes read
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingRead(int         fd,
                void       *buf,
                size_t      count,
                const char *context,
                ssize_t    *bytesReadPtr)
  __attribute__((warn_unused_result));

/**
 * Wrap the pread(2) system call, looping as long as errno is EINTR.
 *
 * @param fd           The descriptor from which to read
 * @param buf          The buffer to read into
 * @param count        The maximum number of bytes to read
 * @param offset       The offset into the file at which to read
 * @param context      The calling context (for logging)
 * @param bytesReadPtr A pointer to hold the number of bytes read
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingPread(int         fd,
                 void       *buf,
                 size_t      count,
                 off_t       offset,
                 const char *context,
                 ssize_t    *bytesReadPtr)
  __attribute__((warn_unused_result));

/**
 * Wrap the write(2) system call.
 *
 * @param fd              The descriptor from which to write
 * @param buf             The buffer to write from
 * @param count           The maximum number of bytes to write
 * @param context         The calling context (for logging)
 * @param bytesWrittenPtr A pointer to hold the number of bytes written;
 *                        on error, -1 is returned
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingWriteInterruptible(int         fd,
                              const void *buf,
                              size_t      count,
                              const char *context,
                              ssize_t    *bytesWrittenPtr)
  __attribute__((warn_unused_result));

/**
 * Wrap the write(2) system call, looping as long as errno is EINTR.
 *
 * @param fd              The descriptor from which to write
 * @param buf             The buffer to write from
 * @param count           The maximum number of bytes to write
 * @param context         The calling context (for logging)
 * @param bytesWrittenPtr A pointer to hold the number of bytes written;
 *                        on error, -1 is returned
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingWrite(int         fd,
                 const void *buf,
                 size_t      count,
                 const char *context,
                 ssize_t    *bytesWrittenPtr)
  __attribute__((warn_unused_result));

/**
 * Wrap the pwrite(2) system call, looping as long as errno is EINTR.
 *
 * @param fd              The descriptor from which to write
 * @param buf             The buffer to write into
 * @param count           The maximum number of bytes to write
 * @param offset          The offset into the file at which to write
 * @param context         The calling context (for logging)
 * @param bytesWrittenPtr A pointer to hold the number of bytes written;
 *                        on error, -1 is returned
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingPwrite(int         fd,
                  const void *buf,
                  size_t      count,
                  off_t       offset,
                  const char *context,
                  ssize_t    *bytesWrittenPtr)
  __attribute__((warn_unused_result));

/**
 * Wrap the close(2) system call.
 *
 * @param fd      The descriptor to close
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int loggingClose(int fd, const char *context)
  __attribute__((warn_unused_result));

/**
 * Perform operations on a process.
 * This wraps the prctl(2) function, q.v.
 *
 * @param option     The operation to perform.
 * @param arg2       Specific to option
 * @param arg3       Specific to option
 * @param arg4       Specific to option
 * @param arg5       Specific to option
 *
 * @return UDS_SUCCESS or an error code
 **/
int processControl(int option, unsigned long arg2, unsigned long arg3,
                   unsigned long arg4, unsigned long arg5);

/**
 * Set up a signal mask, starting with an empty sigset and adding to it the
 * specified signals. If <tt>signals</tt> is NULL, an empty sigset is set up.
 *
 * @param signals An array of signal numbers, the last of which is -1
 * @param sigset  The signal set to set up
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int setUpSigmask(const int *signals, sigset_t *sigset, const char *context)
  __attribute__((warn_unused_result));

/**
 * Set a signal handler.
 *
 * @param signal  The signal to handle
 * @param action  The handler to set
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int setSignalHandler(int signal, struct sigaction *action, const char *context)
  __attribute__((warn_unused_result));

/**
 * Set up to ignore a list of signals.
 *
 * @param signals An array of signal numbers, the last of which is -1
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int ignoreSignals(const int *signals, const char *context)
  __attribute__((warn_unused_result));

/**
 * Restore the default signal handlers for a list of signals.
 *
 * @param signals An array of signal numbers, the last of which is -1
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or an error code
 **/
int restoreSignalHandlers(const int *signals, const char *context)
  __attribute__((warn_unused_result));

/**
 * Set the signal mask.
 *
 * @param mask    The mask to set (may be NULL)
 * @param how     The operation to perform (see sigprocmask(2)).
 * @param context The calling context (for logging)
 * @param oldMask A pointer to hold the previous mask (may be NULL)
 *
 * @return UDS_SUCCESS or an error code
 **/
int setSignalMask(sigset_t   *mask,
                  int         how,
                  const char *context,
                  sigset_t   *oldMask)
  __attribute__((warn_unused_result));

/**
 * Set the signal mask for the current thread.
 *
 * @param mask    The mask to set (may be NULL)
 * @param how     The operation to perform (see pthread_sigmask).
 * @param context The calling context (for logging)
 * @param oldMask A pointer to hold the previous mask (may be NULL)
 *
 * @return UDS_SUCCESS or an error code
 **/
int setThreadSignalMask(sigset_t   *mask,
                        int         how,
                        const char *context,
                        sigset_t   *oldMask)
  __attribute__((warn_unused_result));

/**
 * Daemonizes the current process, and store the resulting PID in a file.
 *
 * If <tt>runDir</tt> could not be created, or switching to it via
 * <tt>chdir</tt> failed, the daemonized process's <tt>runDir</tt> will be the
 * root directory ("/").
 *
 * If <tt>pidFile</tt> already exists and represents a running process, this
 * function will return an error code.
 *
 * @param runDir  The directory to run the daemonized process in
 * @param pidFile The path to the file to store the PID in
 * @param context The calling context (for logging)
 *
 * @return UDS_SUCCESS or error code
 *
 * @note Callers should take care to convert any paths that are relative to
 * absolute paths <b>before</b> calling this function.
 *
 * @see #makeAbsPath
 **/
int daemonize(const char *runDir, const char *pidFile, const char *context)
  __attribute__((warn_unused_result));

/**********************************************************************/
static INLINE int logSystemCallErrno(const char *function, const char *context)
{
  return logWithStringError(((errno == EINTR) ? LOG_DEBUG : LOG_ERR),
                            errno, "%s failed in %s", function, context);
}

/**********************************************************************/
static INLINE int checkSystemCall(int         result,
                                  const char *function,
                                  const char *context)
{
  return ((result == 0) ? UDS_SUCCESS : logSystemCallErrno(function, context));
}

/**********************************************************************/
static INLINE int checkIOErrors(ssize_t bytes,
                                const char *function,
                                const char *context,
                                ssize_t *bytesPtr)
{
  if (bytesPtr != NULL) {
    *bytesPtr = bytes;
  }
  if (bytes < 0) {
    return logSystemCallErrno(function, context);
  }
  return UDS_SUCCESS;
}

#endif /* SYSCALLS_H */
