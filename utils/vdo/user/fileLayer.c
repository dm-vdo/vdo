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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/fileLayer.c#1 $
 */

#include "fileLayer.h"

#include <linux/fs.h>
#include <string.h>
#include <sys/ioctl.h>
#include <zlib.h>

#include "fileUtils.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"
#include "syscalls.h"

#include "constants.h"
#include "statusCodes.h"

typedef struct fileLayer {
  PhysicalLayer common;
  BlockCount    blockCount;
  int           fd;
  char          name[];
} FileLayer;

/**********************************************************************/
static inline FileLayer *asFileLayer(PhysicalLayer *layer)
{
  STATIC_ASSERT(offsetof(FileLayer, common) == 0);
  return (FileLayer *) layer;
}

/**********************************************************************/
static CRC32Checksum updateCRC32(CRC32Checksum  crc,
                                 const byte    *buffer,
                                 size_t         length)
{
  return crc32(crc, buffer, length);
}

/**********************************************************************/
static BlockCount getBlockCount(PhysicalLayer *header)
{
  return asFileLayer(header)->blockCount;
}

/**********************************************************************/
static int bufferAllocator(PhysicalLayer   *header,
                           size_t           bytes,
                           const char      *why,
                           char           **bufferPtr)
{
  if ((bytes % VDO_BLOCK_SIZE) != 0) {
    return logErrorWithStringError(UDS_INVALID_ARGUMENT, "IO buffers must be"
                                   " a multiple of the VDO block size");
  }

  FileLayer *layer = asFileLayer(header);
  struct stat statbuf;
  int result = loggingFstat(layer->fd, &statbuf, __func__);
  if (result != UDS_SUCCESS) {
    return result;
  }

  return allocateMemory(bytes, statbuf.st_blksize, why, bufferPtr);
}

/**********************************************************************/
static int fileReader(PhysicalLayer       *header,
                      PhysicalBlockNumber  startBlock,
                      size_t               blockCount,
                      char                *buffer,
                      size_t              *blocksRead)
{
  FileLayer *layer = asFileLayer(header);

  if (startBlock + blockCount > layer->blockCount) {
    return VDO_OUT_OF_RANGE;
  }

  logDebug("FL: Reading %zu blocks from block %" PRIu64,
           blockCount, startBlock);

  // Make sure we cast so we get a proper 64 bit value on the calculation
  off_t  offset = (off_t)startBlock * VDO_BLOCK_SIZE;
  size_t remain = blockCount * VDO_BLOCK_SIZE;

  while (remain > 0) {
    ssize_t n = pread(layer->fd, buffer, remain, offset);
    if (n <= 0) {
      if (n == 0) {
        errno = VDO_UNEXPECTED_EOF;
      }
      return logErrorWithStringError(errno, "pread %s", layer->name);
    }
    offset += n;
    buffer += n;
    remain -= n;
  }

  if (blocksRead != NULL) {
    *blocksRead = blockCount;
  }
  return VDO_SUCCESS;
}

/**********************************************************************/
static int fileWriter(PhysicalLayer       *header,
                      PhysicalBlockNumber  startBlock,
                      size_t               blockCount,
                      char                *buffer,
                      size_t              *blocksWritten)
{
  FileLayer *layer = asFileLayer(header);

  if (startBlock + blockCount > layer->blockCount) {
    return VDO_OUT_OF_RANGE;
  }

  logDebug("FL: Writing %zu blocks from block %" PRIu64,
           blockCount, startBlock);

  // Make sure we cast so we get a proper 64 bit value on the calculation
  off_t  offset = (off_t)startBlock * VDO_BLOCK_SIZE;
  size_t remain = blockCount * VDO_BLOCK_SIZE;

  while (remain > 0) {
    ssize_t n = pwrite(layer->fd, buffer, remain, offset);
    if (n < 0) {
      return logErrorWithStringError(errno, "pwrite %s", layer->name);
    }
    offset += n;
    buffer += n;
    remain -= n;
  }

  if (blocksWritten != NULL) {
    *blocksWritten = blockCount;
  }

  return VDO_SUCCESS;
}

/**********************************************************************/
static int noWriter(PhysicalLayer       *header __attribute__((unused)),
                    PhysicalBlockNumber  startBlock __attribute__((unused)),
                    size_t               blockCount __attribute__((unused)),
                    char                *buffer __attribute__((unused)),
                    size_t              *blocksWritten __attribute__((unused)))
{
  return EPERM;
}

/**********************************************************************/
static int isBlockDevice(const char *path, bool *device)
{
  struct stat statbuf;
  int result = loggingStatMissingOk(path, &statbuf, __func__);
  if (result == UDS_SUCCESS) {
    *device = (bool) (S_ISBLK(statbuf.st_mode));
  }
  return result;
}

/**********************************************************************/
static void vacuousFlush(VDOFlush **vdoFlush __attribute__((unused)))
{
}

/**
 * Free a FileLayer and NULL out the reference to it.
 *
 * Implements LayerDestructor.
 *
 * @param layerPtr  A pointer to the layer to free
 **/
static void freeLayer(PhysicalLayer **layerPtr)
{
  PhysicalLayer *layer = *layerPtr;
  if (layer == NULL) {
    return;
  }

  FileLayer *fileLayer = asFileLayer(layer);
  trySyncAndCloseFile(fileLayer->fd);
  FREE(fileLayer);
  *layerPtr = NULL;
}

/**
 * Internal constructor to make a file layer.
 *
 * @param [in]  name        the name of the underlying file
 * @param [in]  readOnly    whether the layer is not allowed to write
 * @param [in]  blockCount  the span of the file, in blocks (may be zero for
 *                            read-only layers in which case it is computed)
 * @param [out] layerPtr    the pointer to hold the result
 *
 * @return a success or error code
 **/
static int setupFileLayer(const char     *name,
                          bool            readOnly,
                          BlockCount      blockCount,
                          PhysicalLayer **layerPtr)
{
  int result = ASSERT(layerPtr != NULL, "layerPtr must not be NULL");
  if (result != UDS_SUCCESS) {
    return result;
  }

  size_t     nameLen = strlen(name);
  FileLayer *layer;

  result = ALLOCATE_EXTENDED(FileLayer, nameLen, char, "file layer", &layer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  layer->blockCount = blockCount;
  strcpy(layer->name, name);

  bool exists = false;

  result = fileExists(layer->name, &exists);
  if (result != UDS_SUCCESS) {
    FREE(layer);
    return result;
  }
  if (!exists) {
    FREE(layer);
    return ENOENT;
  }

  FileAccess access = readOnly ? FU_READ_ONLY_DIRECT : FU_READ_WRITE_DIRECT;
  result = openFile(layer->name, access, &layer->fd);
  if (result != UDS_SUCCESS) {
    FREE(layer);
    return result;
  }

  bool blockDevice = false;
  result = isBlockDevice(layer->name, &blockDevice);
  if (result != UDS_SUCCESS) {
    tryCloseFile(layer->fd);
    FREE(layer);
    return result;
  }

  // Make sure the physical blocks == size of the block device
  BlockCount deviceBlocks;
  if (blockDevice) {
    uint64_t bytes;
    if (ioctl(layer->fd, BLKGETSIZE64, &bytes) < 0) {
      result = logErrorWithStringError(errno, "get size of %s", layer->name);
      tryCloseFile(layer->fd);
      FREE(layer);
      return result;
    }
    deviceBlocks = bytes / VDO_BLOCK_SIZE;
  } else {
    struct stat statbuf;
    result = loggingStat(layer->name, &statbuf, __func__);
    if (result != UDS_SUCCESS) {
      tryCloseFile(layer->fd);
      FREE(layer);
      return result;
    }
    deviceBlocks = statbuf.st_size / VDO_BLOCK_SIZE;
  }

  if (layer->blockCount == 0) {
    layer->blockCount = deviceBlocks;
  } else if (layer->blockCount != deviceBlocks) {
    result = logErrorWithStringError(VDO_PARAMETER_MISMATCH,
                                     "physical size %ld 4k blocks must match"
                                     " physical size %ld 4k blocks of %s",
                                     layer->blockCount, deviceBlocks,
                                     layer->name);
    tryCloseFile(layer->fd);
    FREE(layer);
    return result;
  }

  layer->common.destroy             = freeLayer;
  layer->common.updateCRC32         = updateCRC32;
  layer->common.getBlockCount       = getBlockCount;
  layer->common.allocateIOBuffer    = bufferAllocator;
  layer->common.reader              = fileReader;
  layer->common.writer              = readOnly ? noWriter : fileWriter;
  layer->common.completeFlush       = vacuousFlush;

  *layerPtr = &layer->common;
  return VDO_SUCCESS;
}

/**********************************************************************/
int makeFileLayer(const char           *name,
                  BlockCount            blockCount,
                  PhysicalLayer       **layerPtr)
{
  return setupFileLayer(name, false, blockCount, layerPtr);
}

/**********************************************************************/
int makeReadOnlyFileLayer(const char *name, PhysicalLayer **layerPtr)
{
  return setupFileLayer(name, true, 0, layerPtr);
}
