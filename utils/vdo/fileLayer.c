/*
 * Copyright Red Hat
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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/fileLayer.c#14 $
 */

#include "fileLayer.h"

#include <linux/fs.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "fileUtils.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"
#include "syscalls.h"

#include "constants.h"
#include "statusCodes.h"

typedef struct fileLayer {
  PhysicalLayer common;
  block_count_t blockCount;
  int           fd;
  size_t        alignment;
  char          name[];
} FileLayer;

/**********************************************************************/
static inline FileLayer *asFileLayer(PhysicalLayer *layer)
{
  STATIC_ASSERT(offsetof(FileLayer, common) == 0);
  return (FileLayer *) layer;
}

/**********************************************************************/
static block_count_t getBlockCount(PhysicalLayer *header)
{
  return asFileLayer(header)->blockCount;
}

/**********************************************************************/
static int makeAlignedBuffer(FileLayer *layer,
                             char *buffer,
                             size_t bytes,
                             const char *what,
                             char **alignedBufferPtr)
{
  if ((((uintptr_t) buffer) % layer->alignment) == 0) {
    *alignedBufferPtr = buffer;
    return VDO_SUCCESS;
  }

  return allocateIOBuffer(&layer->common, bytes, what, alignedBufferPtr);
}

/**
 * Perform an I/O using a properly aligned buffer.
 *
 * @param [in]     layer       The layer from which to read or write
 * @param [in]     startBlock  The physical block number of the start of the
 *                             extent
 * @param [in]     blockCount  The number of blocks in the extent
 * @param [in]     read        Wether the I/O to perform is a read
 * @param [in/out] buffer      The buffer to read into or write from
 *
 * @return VDO_SUCCESS or an error code
 **/
static int performIO(FileLayer               *layer,
                     physical_block_number_t  startBlock,
                     size_t                   bytes,
                     bool                     read,
                     char                    *buffer)
{
  // Make sure we cast so we get a proper 64 bit value on the calculation
  off_t offset = (off_t) startBlock * VDO_BLOCK_SIZE;
  ssize_t n;
  for (; bytes > 0; bytes -= n) {
    n = (read
         ? pread(layer->fd, buffer, bytes, offset)
         : pwrite(layer->fd, buffer, bytes, offset));
    if (n <= 0) {
      if (n == 0) {
        errno = VDO_UNEXPECTED_EOF;
      }
      return log_error_strerror(errno, "pread %s", layer->name);
    }

    offset += n;
    buffer += n;
  }

  return VDO_SUCCESS;
}

/**********************************************************************/
static int fileReader(PhysicalLayer           *header,
                      physical_block_number_t  startBlock,
                      size_t                   blockCount,
                      char                    *buffer)
{
  FileLayer *layer = asFileLayer(header);

  if (startBlock + blockCount > layer->blockCount) {
    return VDO_OUT_OF_RANGE;
  }

  log_debug("FL: Reading %zu blocks from block %" PRIu64,
	    blockCount, startBlock);

  // Make sure we cast so we get a proper 64 bit value on the calculation
  char *alignedBuffer;
  size_t bytes = VDO_BLOCK_SIZE * blockCount;
  int   result = makeAlignedBuffer(layer, buffer, bytes, "aligned read buffer",
                                   &alignedBuffer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = performIO(layer, startBlock, bytes, true, alignedBuffer);
  if (alignedBuffer != buffer) {
    memcpy(buffer, alignedBuffer, bytes);
    FREE(alignedBuffer);
  }

  return result;
}

/**********************************************************************/
static int fileWriter(PhysicalLayer           *header,
                      physical_block_number_t  startBlock,
                      size_t                   blockCount,
                      char                    *buffer)
{
  FileLayer *layer = asFileLayer(header);

  if (startBlock + blockCount > layer->blockCount) {
    return VDO_OUT_OF_RANGE;
  }

  log_debug("FL: Writing %zu blocks from block %" PRIu64,
	    blockCount, startBlock);

  // Make sure we cast so we get a proper 64 bit value on the calculation
  size_t bytes = blockCount * VDO_BLOCK_SIZE;
  char *alignedBuffer;
  int result = makeAlignedBuffer(layer, buffer, bytes, "aligned write buffer",
                                 &alignedBuffer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  bool wasAligned = (alignedBuffer == buffer);
  if (!wasAligned) {
    memcpy(alignedBuffer, buffer, bytes);
  }

  result = performIO(layer, startBlock, bytes, false, alignedBuffer);
  if (alignedBuffer != buffer) {
    FREE(alignedBuffer);
  }

  return result;
}

/**********************************************************************/
static int
noWriter(PhysicalLayer           *header __attribute__((unused)),
         physical_block_number_t  startBlock __attribute__((unused)),
         size_t                   blockCount __attribute__((unused)),
         char                    *buffer __attribute__((unused)))
{
  return EPERM;
}

/**********************************************************************/
static int isBlockDevice(const char *path, bool *device)
{
  struct stat statbuf;
  int result = logging_stat_missing_ok(path, &statbuf, __func__);
  if (result == UDS_SUCCESS) {
    *device = (bool) (S_ISBLK(statbuf.st_mode));
  }
  return result;
}

/**********************************************************************/
static void vacuousFlush(struct vdo_flush **vdoFlush __attribute__((unused)))
{
}

/**
 * Free a FileLayer and NULL out the reference to it.
 *
 * Implements layer_destructor.
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
  try_sync_and_close_file(fileLayer->fd);
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
                          block_count_t   blockCount,
                          PhysicalLayer **layerPtr)
{
  int result = ASSERT(layerPtr != NULL, "layerPtr must not be NULL");
  if (result != UDS_SUCCESS) {
    return result;
  }

  size_t     nameLen = strlen(name) + 1;
  FileLayer *layer;
  result = ALLOCATE_EXTENDED(FileLayer, nameLen, char, "file layer", &layer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  layer->blockCount = blockCount;
  strcpy(layer->name, name);

  bool exists = false;

  result = file_exists(layer->name, &exists);
  if (result != UDS_SUCCESS) {
    FREE(layer);
    return result;
  }
  if (!exists) {
    FREE(layer);
    return ENOENT;
  }

  enum file_access access
    = readOnly ? FU_READ_ONLY_DIRECT : FU_READ_WRITE_DIRECT;
  result = open_file(layer->name, access, &layer->fd);
  if (result != UDS_SUCCESS) {
    FREE(layer);
    return result;
  }

  bool blockDevice = false;
  result = isBlockDevice(layer->name, &blockDevice);
  if (result != UDS_SUCCESS) {
    try_close_file(layer->fd);
    FREE(layer);
    return result;
  }

  // Determine the block size of the file or device
  struct stat statbuf;
  result = logging_fstat(layer->fd, &statbuf, __func__);
  if (result != UDS_SUCCESS) {
    try_close_file(layer->fd);
    FREE(layer);
    return result;
  }

  // Make sure the physical blocks == size of the block device
  block_count_t deviceBlocks;
  if (blockDevice) {
    uint64_t bytes;
    if (ioctl(layer->fd, BLKGETSIZE64, &bytes) < 0) {
      result = log_error_strerror(errno, "get size of %s", layer->name);
      try_close_file(layer->fd);
      FREE(layer);
      return result;
    }
    deviceBlocks = bytes / VDO_BLOCK_SIZE;
  } else {
    deviceBlocks = statbuf.st_size / VDO_BLOCK_SIZE;
  }

  if (layer->blockCount == 0) {
    layer->blockCount = deviceBlocks;
  } else if (layer->blockCount != deviceBlocks) {
    result = log_error_strerror(VDO_PARAMETER_MISMATCH,
				"physical size %ld 4k blocks must match"
				" physical size %ld 4k blocks of %s",
				layer->blockCount, deviceBlocks,
				layer->name);
    try_close_file(layer->fd);
    FREE(layer);
    return result;
  }

  layer->alignment               = statbuf.st_blksize;
  layer->common.destroy          = freeLayer;
  layer->common.getBlockCount    = getBlockCount;
  layer->common.reader           = fileReader;
  layer->common.writer           = readOnly ? noWriter : fileWriter;
  layer->common.completeFlush    = vacuousFlush;

  *layerPtr = &layer->common;
  return VDO_SUCCESS;
}

/**********************************************************************/
int makeFileLayer(const char           *name,
                  block_count_t         blockCount,
                  PhysicalLayer       **layerPtr)
{
  return setupFileLayer(name, false, blockCount, layerPtr);
}

/**********************************************************************/
int makeReadOnlyFileLayer(const char *name, PhysicalLayer **layerPtr)
{
  return setupFileLayer(name, true, 0, layerPtr);
}

/**********************************************************************/
int allocateIOBuffer(PhysicalLayer   *header,
                     size_t           bytes,
                     const char      *why,
                     char           **bufferPtr)
{
  if ((bytes % VDO_BLOCK_SIZE) != 0) {
    return log_error_strerror(UDS_INVALID_ARGUMENT, "IO buffers must be"
			      " a multiple of the VDO block size");
  }

  return allocate_memory(bytes,
                         asFileLayer(header)->alignment,
                         why,
                         bufferPtr);
}
