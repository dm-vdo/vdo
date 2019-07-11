/*
 * Copyright (c) 2019 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/jasper/userLinux/uds/fileIORegion.c#2 $
 */

#include "fileIORegion.h"

#include "compiler.h"
#include "ioFactory.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"

typedef struct fileIORegion {
  IORegion   common;
  IOFactory *factory;
  int        fd;
  bool       reading;
  bool       writing;
  size_t     blockSize;
  size_t     bestSize;
} FileIORegion;

/*****************************************************************************/
static INLINE FileIORegion *asFileIORegion(IORegion *region)
{
  return container_of(region, FileIORegion, common);
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
  int result = putIOFactory(fior->factory);
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

  size_t dataLength = 0;
  result = readDataAtOffset(fior->fd, offset, buffer, size, &dataLength);
  if (result != UDS_SUCCESS) {
    return result;
  }
  if (length == NULL) {
    if (dataLength < size) {
      byte *buf = buffer;
      memset(&buf[dataLength], 0, size - dataLength);
    }
    return result;
  }

  if (dataLength < *length) {
    if (dataLength == 0) {
      return logErrorWithStringError(UDS_END_OF_FILE,
                                     "expected at least %zu bytes, got EOF",
                                     len);
    } else {
      return logErrorWithStringError(UDS_SHORT_READ,
                                     "expected at least %zu bytes, got %zu",
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
int makeFileRegion(IOFactory   *factory,
                   int          fd,
                   FileAccess   access,
                   IORegion   **regionPtr)
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

  getIOFactory(factory);

  fior->common.clear        = fior_clear;
  fior->common.close        = fior_close;
  fior->common.getBestSize  = fior_getBestSize;
  fior->common.getBlockSize = fior_getBlockSize;
  fior->common.getDataSize  = fior_getDataSize;
  fior->common.getLimit     = fior_getLimit;
  fior->common.read         = fior_read;
  fior->common.syncContents = fior_syncContents;
  fior->common.write        = fior_write;
  fior->factory   = factory;
  fior->fd        = fd;
  fior->reading   = (access <= FU_CREATE_READ_WRITE);
  fior->writing   = (access >= FU_READ_WRITE);
  fior->blockSize = blockSize;
  fior->bestSize  = bestSize;
  *regionPtr = &fior->common;
  return UDS_SUCCESS;
}
