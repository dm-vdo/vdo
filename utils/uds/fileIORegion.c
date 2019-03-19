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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/fileIORegion.c#2 $
 */

#include "fileIORegion.h"

#include "compiler.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"

typedef struct fileIORegion {
  IORegion common;
  int      fd;
  bool     close;
  bool     reading;
  bool     writing;
  size_t   blockSize;
  size_t   bestSize;
} FileIORegion;

/*****************************************************************************/
static INLINE FileIORegion *asFileIORegion(IORegion *region)
{
  return container_of(region, FileIORegion, common);
}

/*****************************************************************************/
int openFileRegion(const char *path, FileAccess access, IORegion **regionPtr)
{
  int fd = -1;
  int result = openFile(path, access, &fd);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = makeFileRegion(fd, access, regionPtr);
  if (result != UDS_SUCCESS) {
    tryCloseFile(fd);
    return result;
  }

  setFileRegionCloseBehavior(*regionPtr, true);
  return UDS_SUCCESS;
}

/*****************************************************************************/
int setFileRegionLimit(IORegion *region, off_t limit)
{
  FileIORegion *fior = asFileIORegion(region);

  if (!fior->writing) {
    return logErrorWithStringError(UDS_BAD_IO_DIRECTION,
                                   "cannot set limit on read-only file");
  }

  off_t current = 0;
  int result = getOpenFileSize(fior->fd, &current);
  if (result != UDS_SUCCESS) {
    return result;
  }

  if (limit > current) {
    result = setOpenFileSize(fior->fd, limit);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
void setFileRegionCloseBehavior(IORegion *region, bool closeFile)
{
  FileIORegion *fior = asFileIORegion(region);
  fior->close = closeFile;
}

/*****************************************************************************/
static int validateIO(FileIORegion *fior,
                      off_t         offset,
                      size_t        size,
                      size_t        length,
                      bool          willWrite)
{
  if (!(willWrite ? fior->writing : fior->reading)) {
    return logErrorWithStringError(UDS_BAD_IO_DIRECTION,
                                   "not open for %s",
                                   willWrite ? "writing" : "reading");
  }

  if (offset % fior->blockSize != 0) {
    return logErrorWithStringError(UDS_INCORRECT_ALIGNMENT,
                                   "alignment %zd not multiple of %zd", offset,
                                   fior->blockSize);
  }

  if (size % fior->blockSize != 0) {
    return logErrorWithStringError(UDS_BUFFER_ERROR,
                                   "buffer size %zd not a multiple of %zd",
                                   size, fior->blockSize);
  }

  if (length > size) {
    return logErrorWithStringError(UDS_BUFFER_ERROR,
                                   "length %zd exceeds buffer size %zd",
                                   length, size);
  }

  return UDS_SUCCESS;
}

/*****************************************************************************/
static int fior_close(IORegion *region)
{
  FileIORegion *fior = asFileIORegion(region);

  int result = UDS_SUCCESS;

  if (fior->close) {
    result = closeFile(fior->fd, NULL);
  }
  FREE(fior);
  return result;
}

/*****************************************************************************/
static int fior_getLimit(IORegion *region __attribute__((unused)),
                         off_t    *limit)
{
  *limit = INT64_MAX;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int fior_getDataSize(IORegion *region,
                            off_t    *extent)
{
  FileIORegion *fior = asFileIORegion(region);

  return getOpenFileSize(fior->fd, extent);
}

/*****************************************************************************/
static int fior_clear(IORegion *region)
{
  FileIORegion *fior = asFileIORegion(region);

  int result = validateIO(fior, 0, 0, 0, true);
  if (result != UDS_SUCCESS) {
    return result;
  }

  return setOpenFileSize(fior->fd, 0);
}

/*****************************************************************************/
static int fior_write(IORegion   *region,
                      off_t       offset,
                      const void *data,
                      size_t      size,
                      size_t      length)
{
  FileIORegion *fior = asFileIORegion(region);

  int result = validateIO(fior, offset, size, length, true);
  if (result != UDS_SUCCESS) {
    return result;
  }

  return writeBufferAtOffset(fior->fd, offset, data, length);
}

/*****************************************************************************/
static int fior_read(IORegion *region,
                     off_t     offset,
                     void     *buffer,
                     size_t    size,
                     size_t   *length)
{
  FileIORegion *fior = asFileIORegion(region);

  size_t len = (length == NULL) ? size : *length;

  int result = validateIO(fior, offset, size, len, false);
  if (result != UDS_SUCCESS) {
    return result;
  }

  if (length == NULL) {
    return readBufferAtOffset(fior->fd, offset, buffer, size);
  }

  unsigned int dataLength = 0;
  result = readDataAtOffset(fior->fd, offset, buffer, size, &dataLength);
  if (result != UDS_SUCCESS) {
    return result;
  }

  if (dataLength < *length) {
    if (dataLength == 0) {
      return logErrorWithStringError(UDS_END_OF_FILE,
                                     "expected at least %zd bytes, got EOF",
                                     len);
    } else {
      return logErrorWithStringError(UDS_SHORT_READ,
                                     "expected at least %zd bytes, got %u",
                                     len, dataLength);
    }
  }
  *length = dataLength;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int fior_getBlockSize(IORegion *region, size_t *blockSize)
{
  *blockSize = asFileIORegion(region)->blockSize;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int fior_getBestSize(IORegion *region, size_t *bufferSize)
{
  *bufferSize = asFileIORegion(region)->bestSize;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int fior_syncContents(IORegion *region)
{
  FileIORegion *fior = asFileIORegion(region);

  return loggingFsync(fior->fd, "cannot sync contents of file IORegion");
}

/*****************************************************************************/
int makeFileRegion(int fd, FileAccess access, IORegion **regionPtr)
{
  size_t blockSize = 1024;
  size_t bestSize  = 4096;

  int result = getBufferSizeInfo(bestSize, &blockSize, &bestSize);
  if (result != UDS_SUCCESS) {
    return result;
  }

  FileIORegion *fior;
  result = ALLOCATE(1, FileIORegion, "open file region", &fior);
  if (result != UDS_SUCCESS) {
    return result;
  }
  fior->common.clear        = fior_clear;
  fior->common.close        = fior_close;
  fior->common.getBestSize  = fior_getBestSize;
  fior->common.getBlockSize = fior_getBlockSize;
  fior->common.getDataSize  = fior_getDataSize;
  fior->common.getLimit     = fior_getLimit;
  fior->common.read         = fior_read;
  fior->common.syncContents = fior_syncContents;
  fior->common.write        = fior_write;
  fior->fd          = fd;
  fior->close       = false;
  fior->reading     = (access <= FU_CREATE_READ_WRITE);
  fior->writing     = (access >= FU_READ_WRITE);
  fior->blockSize   = blockSize;
  fior->bestSize    = bestSize;
  *regionPtr = &fior->common;
  return UDS_SUCCESS;
}
