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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/fileUtils.c#1 $
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

enum { MINIMUM_BLOCK_SIZE = 4096 };

/**********************************************************************/
int getBufferSizeInfo(size_t  defaultBestSize,
                      size_t *blockSizePtr,
                      size_t *bestSizePtr)
{
  size_t blockSize = MINIMUM_BLOCK_SIZE;

  if (blockSizePtr != NULL) {
    *blockSizePtr = blockSize;
  }
  if (bestSizePtr != NULL) {
    *bestSizePtr = leastCommonMultiple(blockSize, defaultBestSize);
  }
  return UDS_SUCCESS;
}

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
  int result = syncAndCloseFile(fd, NULL);
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

static unsigned int REQUIRE_FULL_READ = -1;     // value unimportant

/**********************************************************************/
static int readBufferAtOffsetCommon(int           fd,
                                    off_t         offset,
                                    void         *buffer,
                                    unsigned int  length,
                                    unsigned int *howMany)
{
  byte *ptr = buffer;
  size_t bytesToRead = length;
  off_t currentOffset = offset;

  while (bytesToRead > 0) {
    ssize_t bytesRead;
    int result = loggingPread(fd, ptr, bytesToRead, currentOffset, __func__,
                              &bytesRead);
    if (result != UDS_SUCCESS) {
      return result;
    }

    if (bytesRead == 0) {
      if (howMany == &REQUIRE_FULL_READ) {
        return logWarningWithStringError(UDS_CORRUPT_FILE,
                                         "unexpected end of file while reading"
                                         " at 0x%" PRIx64,
                                         (uint64_t) currentOffset);
      } else if (howMany == NULL) {
        return UDS_END_OF_FILE;
      } else {
        break;
      }
    }
    ptr += bytesRead;
    bytesToRead -= bytesRead;
    currentOffset += bytesRead;
  }
  if ((howMany != NULL) && (howMany != &REQUIRE_FULL_READ)) {
    *howMany = ptr - (byte *) buffer;
  }

  return UDS_SUCCESS;
}

/**********************************************************************/
int readBufferAtOffset(int           fd,
                       off_t         offset,
                       void         *buffer,
                       unsigned int  length)
{
  return readBufferAtOffsetCommon(fd, offset, buffer, length,
                                  &REQUIRE_FULL_READ);
}

/**********************************************************************/
int readDataAtOffset(int           fd,
                     off_t         offset,
                     void         *buffer,
                     unsigned int  size,
                     unsigned int *length)
{
  return readBufferAtOffsetCommon(fd, offset, buffer, size, length);
}

/**********************************************************************/
int readAndVerify(int           fd,
                  const byte   *requiredValue,
                  unsigned int  length)
{
  byte buffer[length];
  int result = readBuffer(fd, buffer, length);
  if (result != UDS_SUCCESS) {
    return result;
  }
  if (memcmp(requiredValue, buffer, length) != 0) {
    return logWarningWithStringError(UDS_CORRUPT_FILE, "%s got wrong data",
                                     __func__);
  }
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
int setOpenFileSize(int fd, off_t size)
{
  int r = ftruncate(fd, size);
  if (r == 0) {
    return UDS_SUCCESS;
  }
  return logWarningWithStringError(errno, "cannot truncate open file");
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

/**
 * Stat a file logging any errors.
 *
 * @param path      The path to the file to stat
 * @param buf       A pointer to hold the result
 * @param function  The calling function
 * @param context   The calling context
 * @param missingOk If <code>true</code>, it is not an error for the
 *                  file being stated to not exist
 *
 * @return UDS_SUCCESS or an error code
 **/
static int loggingStatInternal(const char  *path,
                               struct stat *buf,
                               const char  *function,
                               const char  *context,
                               bool         missingOk)
{
  if (stat(path, buf) == 0) {
    return UDS_SUCCESS;
  }

  if (missingOk && (errno == ENOENT)) {
    return errno;
  }

  return logErrorWithStringError(errno, "%s failed in %s for path %s",
                                 function, context, path);
}

/**********************************************************************/
int loggingStat(const char *path, struct stat *buf, const char *context)
{
  return loggingStatInternal(path, buf, __func__, context, false);
}

/**********************************************************************/
int loggingStatMissingOk(const char  *path,
                         struct stat *buf,
                         const char  *context)
{
  return loggingStatInternal(path, buf, __func__, context, true);
}

/**********************************************************************/
int loggingFstat(int fd, struct stat *buf, const char *context)
{
  return checkSystemCall(fstat(fd, buf), __func__, context);
}

/**********************************************************************/
int loggingFcntl(int fd, int cmd, const char *context, long *value)
{
  int result = fcntl(fd, cmd);
  if (result == -1) {
    return logErrorWithStringError(errno,
                                   "%s failed in %s on fd %d, command %d",
                                   __func__, context, fd, cmd);
  }

  if (value != NULL) {
    *value = result;
  }
  return UDS_SUCCESS;
}

/**********************************************************************/
int loggingFcntlWithArg(int         fd,
                        int         cmd,
                        long        arg,
                        const char *context,
                        long       *value)
{
  int result = fcntl(fd, cmd, arg);
  if (result != -1) {
    if (value != NULL) {
      *value = result;
    }
    return UDS_SUCCESS;
  }

  return
    logErrorWithStringError(errno,
                            "%s failed in %s on fd %d, command %d, arg: %ld",
                            __func__, context, fd, cmd, arg);
}

/**********************************************************************/
int loggingFsync(int fd, const char *context)
{
  return checkSystemCall(fsync(fd), __func__, context);
}

/**********************************************************************/
int loggingLseek(int         fd,
                 off_t       offset,
                 int         whence,
                 const char *context,
                 off_t      *offsetPtr)
{
  off_t newOffset = lseek(fd, offset, whence);
  if (newOffset == -1) {
    return logErrorWithStringError(errno,
                                   "%s failed in %s on fd %d, offset: %zu, "
                                   "whence: %d",
                                   __func__, context, fd, offset, whence);
  }

  if (offsetPtr != NULL) {
    *offsetPtr = newOffset;
  }
  return UDS_SUCCESS;
}

/**********************************************************************/
int makeDirectory(const char *path,
                  mode_t      mode,
                  const char *directoryType,
                  const char *context)
{
  int result = mkdir(path, mode);
  if (result == 0) {
    return UDS_SUCCESS;
  }

  return logWithStringError(((errno == EEXIST) ? LOG_WARNING : LOG_ERR),
                            errno, "%s failed in %s making %s directory %s",
                            __func__, context, directoryType, path);
}

/**********************************************************************/
int loggingPathconf(const char *path,
                    int         name,
                    const char *context,
                    long       *value)
{
  errno = 0;
  long result = pathconf(path, name);
  if ((result < 0) && (errno != 0)) {
    return logErrorWithStringError(errno,
                                   "%s failed in %s on path %s, option %d",
                                   __func__, context, path, name);
  }

  *value = result;
  return UDS_SUCCESS;
}

/**********************************************************************/
int loggingRename(const char *oldPath,
                  const char *newPath,
                  const char *context)
{
  int result = rename(oldPath, newPath);
  if (result != 0) {
    return logErrorWithStringError(errno,
                                   "%s failed in %s, oldPath: %s, newPath: %s",
                                   __func__, context, oldPath, newPath);
  }

  return UDS_SUCCESS;
}
