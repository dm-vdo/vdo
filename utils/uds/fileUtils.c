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
 * $Id: //eng/uds-releases/jasper/userLinux/uds/fileUtils.c#6 $
 */

#include "fileUtils.h"

#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "errors.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "numeric.h"
#include "permassert.h"
#include "stringUtils.h"
#include "syscalls.h"

/**********************************************************************/
int fileExists(const char *path, bool *exists)
{
  struct stat statBuf;
  int result = loggingStatMissingOk(path, &statBuf, __func__);

  if (result == UDS_SUCCESS) {
    *exists = true;
  } else if (result == ENOENT) {
    *exists = false;
    result = UDS_SUCCESS;
  }

  return result;
}

/**********************************************************************/
int openFile(const char *path, FileAccess access, int *fd)
{
  int retFd;
  int flags;
  mode_t mode;

  switch (access) {
  case FU_READ_ONLY:
    flags = O_RDONLY;
    mode = 0;
    break;
  case FU_READ_WRITE:
    flags = O_RDWR;
    mode = 0;
    break;
  case FU_CREATE_READ_WRITE:
    flags = O_CREAT | O_RDWR | O_TRUNC;
    mode = 0666;
    break;
  case FU_CREATE_WRITE_ONLY:
    flags = O_CREAT | O_WRONLY | O_TRUNC;
    mode = 0666;
    break;
  case FU_READ_ONLY_DIRECT:
    flags = O_RDONLY | O_DIRECT;
    mode = 0;
    break;
  case FU_READ_WRITE_DIRECT:
    flags = O_RDWR | O_DIRECT;
    mode = 0;
    break;
  case FU_CREATE_READ_WRITE_DIRECT:
    flags = O_CREAT | O_RDWR | O_TRUNC | O_DIRECT;
    mode = 0666;
    break;
  case FU_CREATE_WRITE_ONLY_DIRECT:
    flags = O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT;
    mode = 0666;
    break;
  default:
    return logWarningWithStringError(UDS_INVALID_ARGUMENT,
                                     "invalid access mode opening file %s",
                                     path);
  }

  do {
    retFd = open(path, flags, mode);
  } while ((retFd == -1) && (errno == EINTR));
  if (retFd < 0) {
    return logErrorWithStringError(errno, "openFile(): failed opening %s "
                                   "with file access: %d",
                                   path, access);
  }
  *fd = retFd;
  return UDS_SUCCESS;
}

/**********************************************************************/
int closeFile(int fd, const char *errorMessage)
{
  return loggingClose(fd, errorMessage);
}

/**********************************************************************/
void tryCloseFile(int fd)
{
  int oldErrno = errno;
  int result = closeFile(fd, __func__);
  errno = oldErrno;
  if (result != UDS_SUCCESS) {
    logDebugWithStringError(result, "error closing file");
  }
}

/**********************************************************************/
int syncAndCloseFile(int fd, const char *errorMessage)
{
  int result = loggingFsync(fd, errorMessage);
  if (result != UDS_SUCCESS) {
    tryCloseFile(fd);
    return result;
  }
  return closeFile(fd, errorMessage);
}

/**********************************************************************/
void trySyncAndCloseFile(int fd)
{
  int result = syncAndCloseFile(fd, __func__);
  if (result != UDS_SUCCESS) {
    logDebugWithStringError(result, "error syncing and closing file");
  }
}

/**********************************************************************/
int readBuffer(int fd, void *buffer, unsigned int length)
{
  byte *ptr = buffer;
  size_t bytesToRead = length;

  while (bytesToRead > 0) {
    ssize_t bytesRead;
    int result = loggingRead(fd, ptr, bytesToRead, __func__, &bytesRead);
    if (result != UDS_SUCCESS) {
      return result;
    }

    if (bytesRead == 0) {
      return logWarningWithStringError(UDS_CORRUPT_FILE,
                                       "unexpected end of file while reading");
    }

    ptr += bytesRead;
    bytesToRead -= bytesRead;
  }

  return UDS_SUCCESS;
}

/**********************************************************************/
int readDataAtOffset(int     fd,
                     off_t   offset,
                     void   *buffer,
                     size_t  size,
                     size_t *length)
{
  byte *ptr = buffer;
  size_t bytesToRead = size;
  off_t currentOffset = offset;

  while (bytesToRead > 0) {
    ssize_t bytesRead;
    int result = loggingPread(fd, ptr, bytesToRead, currentOffset, __func__,
                              &bytesRead);
    if (result != UDS_SUCCESS) {
      return result;
    }

    if (bytesRead == 0) {
      break;
    }
    ptr += bytesRead;
    bytesToRead -= bytesRead;
    currentOffset += bytesRead;
  }

  *length = ptr - (byte *) buffer;
  return UDS_SUCCESS;
}


/**********************************************************************/
int writeBuffer(int           fd,
                const void   *buffer,
                unsigned int  length)
{
  size_t bytesToWrite = length;
  const byte *ptr = buffer;
  while (bytesToWrite > 0) {
    ssize_t written;
    int result = loggingWrite(fd, ptr, bytesToWrite, __func__, &written);
    if (result != UDS_SUCCESS) {
      return result;
    }

    if (written == 0) {
      // this should not happen, but if it does, errno won't be defined, so we
      // need to return our own error
      return logErrorWithStringError(UDS_UNKNOWN_ERROR, "wrote 0 bytes");
    }
    bytesToWrite -= written;
    ptr += written;
  }
  return UDS_SUCCESS;
}

/**********************************************************************/
int writeBufferAtOffset(int           fd,
                        off_t         offset,
                        const void   *buffer,
                        unsigned int  length)
{
  size_t bytesToWrite = length;
  const byte *ptr = buffer;
  off_t currentOffset = offset;

  while (bytesToWrite > 0) {
    ssize_t written;
    int result = loggingPwrite(fd, ptr, bytesToWrite, currentOffset, __func__,
                               &written);
    if (result != UDS_SUCCESS) {
      return result;
    }

    if (written == 0) {
      // this should not happen, but if it does, errno won't be defined, so we
      // need to return our own error
      return logErrorWithStringError(UDS_UNKNOWN_ERROR,
                                     "impossible write error");
    }

    bytesToWrite -= written;
    ptr += written;
    currentOffset += written;
  }

  return UDS_SUCCESS;
}

/**********************************************************************/
int getOpenFileSize(int fd, off_t *sizePtr)
{
  struct stat statbuf;

  if (loggingFstat(fd, &statbuf, "getOpenFileSize()") == -1) {
    return errno;
  }
  *sizePtr = statbuf.st_size;
  return UDS_SUCCESS;
}

/**********************************************************************/
int removeFile(const char *fileName)
{
  int result = unlink(fileName);
  if (result == 0 || errno == ENOENT) {
    return UDS_SUCCESS;
  }
  return logWarningWithStringError(errno, "Failed to remove %s", fileName);
}

/**********************************************************************/
bool fileNameMatch(const char *pattern, const char *string, int flags)
{
  int result = fnmatch(pattern, string, flags);
  if ((result != 0) && (result != FNM_NOMATCH)) {
    logError("fileNameMatch(): fnmatch(): returned an error: %d, "
             "looking for \"%s\" with flags: %d", result, string, flags);
  }
  return (result == 0);
}

/**********************************************************************/
int makeAbsPath(const char *path, char **absPath)
{
  char *tmp;
  int result = UDS_SUCCESS;
  if (path[0] == '/') {
    result = duplicateString(path, __func__, &tmp);
  } else {
    char *cwd = get_current_dir_name();
    if (cwd == NULL) {
      return errno;
    }
    result = allocSprintf(__func__, &tmp, "%s/%s", cwd, path);
    FREE(cwd);
  }
  if (result == UDS_SUCCESS) {
    *absPath = tmp;
  }
  return result;
}

/**********************************************************************/
int loggingStat(const char *path, struct stat *buf, const char *context)
{
  if (stat(path, buf) == 0) {
    return UDS_SUCCESS;
  }
  return logErrorWithStringError(errno, "%s failed in %s for path %s",
                                 __func__, context, path);
}

/**********************************************************************/
int loggingStatMissingOk(const char  *path,
                         struct stat *buf,
                         const char  *context)
{
  if (stat(path, buf) == 0) {
    return UDS_SUCCESS;
  }
  if (errno == ENOENT) {
    return errno;
  }
  return logErrorWithStringError(errno, "%s failed in %s for path %s",
                                 __func__, context, path);
}

/**********************************************************************/
int loggingFstat(int fd, struct stat *buf, const char *context)
{
  return checkSystemCall(fstat(fd, buf), __func__, context);
}

/**********************************************************************/
int loggingFsync(int fd, const char *context)
{
  return checkSystemCall(fsync(fd), __func__, context);
}
