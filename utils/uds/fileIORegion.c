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
 * $Id: //eng/uds-releases/jasper/userLinux/uds/fileIORegion.c#10 $
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
  off_t      offset;
  size_t     size;
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

  if (length > size) {
    return logErrorWithStringError(UDS_BUFFER_ERROR,
                                   "length %zd exceeds buffer size %zd",
                                   length, size);
  }

  if (offset + length > fior->size) {
    return logErrorWithStringError(UDS_OUT_OF_RANGE,
                                   "range %zd-%zd not in range 0 to %zu",
                                   offset, offset + length, fior->size);
  }

  return UDS_SUCCESS;
}

/*****************************************************************************/
static void fior_free(IORegion *region)
{
  FileIORegion *fior = asFileIORegion(region);
  putIOFactory(fior->factory);
  FREE(fior);
}

/*****************************************************************************/
static int fior_getLimit(IORegion *region __attribute__((unused)),
                         off_t    *limit)
{
  FileIORegion *fior = asFileIORegion(region);
  *limit = fior->size;
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

  return writeBufferAtOffset(fior->fd, fior->offset + offset, data, length);
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
  result = readDataAtOffset(fior->fd, fior->offset + offset, buffer, size,
                            &dataLength);
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
static int fior_syncContents(IORegion *region)
{
  FileIORegion *fior = asFileIORegion(region);
  return loggingFsync(fior->fd, "cannot sync contents of file IORegion");
}

/*****************************************************************************/
int makeFileRegion(IOFactory   *factory,
                   int          fd,
                   FileAccess   access,
                   off_t        offset,
                   size_t       size,
                   IORegion   **regionPtr)
{
  FileIORegion *fior;
  int result = ALLOCATE(1, FileIORegion, __func__, &fior);
  if (result != UDS_SUCCESS) {
    return result;
  }

  getIOFactory(factory);

  fior->common.free         = fior_free;
  fior->common.getDataSize  = fior_getDataSize;
  fior->common.getLimit     = fior_getLimit;
  fior->common.read         = fior_read;
  fior->common.syncContents = fior_syncContents;
  fior->common.write        = fior_write;

  fior->factory  = factory;
  fior->fd       = fd;
  fior->reading  = (access <= FU_CREATE_READ_WRITE);
  fior->writing  = (access >= FU_READ_WRITE);
  fior->offset   = offset;
  fior->size     = size;

  atomic_set_release(&fior->common.refCount, 1);
  *regionPtr = &fior->common;
  return UDS_SUCCESS;
}
