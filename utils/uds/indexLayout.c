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
 * $Id: //eng/uds-releases/jasper/src/uds/indexLayout.c#19 $
 */

#include "indexLayout.h"

#include "buffer.h"
#include "compiler.h"
#include "config.h"
#include "indexConfig.h"
#include "layoutRegion.h"
#include "logger.h"
#include "masterIndexOps.h"
#include "memoryAlloc.h"
#include "nonce.h"
#include "openChapter.h"

/*
 * Overall layout of an index on disk:
 *
 * The layout is divided into a number of fixed-size regions, the sizes of
 * which are computed when the index is created. Every header and region
 * begins on 4K block boundary. Save regions are further sub-divided into
 * regions of their own.
 *
 * Each region has a kind and an instance number. Some kinds only have one
 * instance and therefore use RL_SOLE_INSTANCE (-1) as the instance number.
 * The RL_KIND_INDEX uses instances to represent sub-indices, where used.
 * A save region can either hold a checkpoint or a clean shutdown (determined
 * by the type). The instances determine which available save slot is used.
 * The RL_KIND_MASTER_INDEX uses instances to record which zone is being saved.
 *
 *     +-+-+--------+--------+--------+-----+---  -+-+
 *     | | |   I N D E X   0      101, 0    | ...  | |
 *     |H|C+--------+--------+--------+-----+---  -+S|
 *     |D|f| Volume | Save   | Save   |     |      |e|
 *     |R|g| Region | Region | Region | ... | ...  |a|
 *     | | | 201 -1 | 202  0 | 202  1 |     |      |l|
 *     +-+-+--------+--------+--------+-----+---  -+-+
 *
 * The header contains the encoded regional layout table as well as
 * the saved index configuration record. The sub-index regions and their
 * subdivisions are maintained in the same table.
 *
 * There are at least two save regions per sub-index to preserve the old
 * state should the saving of a state be incomplete. They are used in
 * a round-robin fashion.
 *
 * Anatomy of a save region:
 *
 *     +-+-----+------+------+-----+   -+-----+
 *     |H| IPM | MI   | MI   |     |    | OC  |
 *     |D|     | zone | zone | ... |    |     |
 *     |R| 301 | 302  | 302  |     |    | 303 |
 *     | | -1  | 0    | 1    |     |    | -1  |
 *     +-+-----+------+------+-----+   -+-----+
 *
 * Every region header has a type (and version). In save regions,
 * the open chapter only appears in RL_TYPE_SAVE not RL_TYPE_CHECKPOINT,
 * although the same space is reserved for both.
 *
 * The header contains the encoded regional layout table as well as the
 * index state record for that save or checkpoint. Each save or checkpoint
 * has a unique generation number and nonce which is used to seed the
 * checksums of those regions.
 */

typedef struct indexSaveData_v1 {
  uint64_t timestamp;           // ms since epoch...
  uint64_t nonce;
  uint32_t version;             // 1
  uint32_t unused__; 
} IndexSaveData;

typedef struct indexSaveLayout {
  LayoutRegion     indexSave;
  LayoutRegion     header;
  unsigned int     numZones;
  LayoutRegion     indexPageMap;
  LayoutRegion     freeSpace;
  LayoutRegion    *masterIndexZones;
  LayoutRegion    *openChapter;
  IndexSaveType    saveType;
  IndexSaveData    saveData;
  Buffer          *indexStateBuffer;
  bool             read;
  bool             written;
} IndexSaveLayout;

typedef struct subIndexLayout {
  LayoutRegion     subIndex;
  uint64_t         nonce;
  LayoutRegion     volume;
  IndexSaveLayout *saves;
} SubIndexLayout;

typedef struct superBlockData_v1 {
  byte     magicLabel[32];
  byte     nonceInfo[32];
  uint64_t nonce;
  uint32_t version;             // 2
  uint32_t blockSize;           // for verification
  uint16_t numIndexes;          // 1
  uint16_t maxSaves;
  uint64_t openChapterBlocks;
  uint64_t pageMapBlocks;
} SuperBlockData;

struct indexLayout {
  IOFactory            *factory;
  off_t                 offset;
  struct index_version  indexVersion;
  SuperBlockData        super;
  LayoutRegion          header;
  LayoutRegion          config;
  SubIndexLayout        index;
  LayoutRegion          seal;
  uint64_t              totalBlocks;
  int                   refCount;
};

/**
 * Structure used to compute single file layout sizes.
 *
 * Note that the masterIndexBlocks represent all zones and are sized for
 * the maximum number of blocks that would be needed regardless of the number
 * of zones (up to the maximum value) that are used at run time.
 *
 * Similarly, the number of saves is sized for the minimum safe value
 * assuming checkpointing is enabled, since that is also a run-time parameter.
 **/
typedef struct saveLayoutSizes {
  Configuration config;                 // this is a captive copy
  Geometry      geometry;               // this is a captive copy
  unsigned int  numSaves;               // per sub-index
  size_t        blockSize;              // in bytes
  uint64_t      volumeBlocks;           // per sub-index
  uint64_t      masterIndexBlocks;      // per save
  uint64_t      pageMapBlocks;          // per save
  uint64_t      openChapterBlocks;      // per save
  uint64_t      saveBlocks;             // per sub-index
  uint64_t      subIndexBlocks;         // per sub-index
  uint64_t      totalBlocks;            // for whole layout
} SaveLayoutSizes;

enum {
  INDEX_STATE_BUFFER_SIZE =  512,
  MAX_SAVES               =    5,
};

static const byte SINGLE_FILE_MAGIC_1[32] = "*ALBIREO*SINGLE*FILE*LAYOUT*001*";
enum {
  SINGLE_FILE_MAGIC_1_LENGTH = sizeof(SINGLE_FILE_MAGIC_1),
};

static int reconstituteSingleFileLayout(IndexLayout    *layout,
                                        SuperBlockData *super,
                                        RegionTable    *table,
                                        uint64_t        firstBlock)
  __attribute__((warn_unused_result));
static int writeIndexSaveLayout(IndexLayout *layout, IndexSaveLayout *isl)
  __attribute__((warn_unused_result));

/*****************************************************************************/
static INLINE uint64_t blockCount(uint64_t bytes, uint32_t blockSize)
{
  uint64_t blocks = bytes / blockSize;
  if (bytes % blockSize > 0) {
    ++blocks;
  }
  return blocks;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int computeSizes(SaveLayoutSizes        *sls,
                        const UdsConfiguration  config,
                        size_t                  blockSize,
                        unsigned int            numCheckpoints)
{
  if (config->bytesPerPage % blockSize != 0) {
    return logErrorWithStringError(UDS_INCORRECT_ALIGNMENT,
                                   "page size not a multiple of block size");
  }

  Configuration *cfg = NULL;
  int result = makeConfiguration(config, &cfg);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "cannot compute layout size");
  }

  memset(sls, 0, sizeof(*sls));

  // internalize the configuration and geometry...

  sls->geometry        = *cfg->geometry;
  sls->config          = *cfg;
  sls->config.geometry = &sls->geometry;

  freeConfiguration(cfg);

  sls->numSaves         = 2 + numCheckpoints;
  sls->blockSize        = blockSize;
  sls->volumeBlocks     = sls->geometry.bytesPerVolume / blockSize;

  result = computeMasterIndexSaveBlocks(&sls->config, blockSize,
                                        &sls->masterIndexBlocks);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "cannot compute index save size");
  }

  sls->pageMapBlocks =
    blockCount(computeIndexPageMapSaveSize(&sls->geometry), blockSize);
  sls->openChapterBlocks =
    blockCount(computeSavedOpenChapterSize(&sls->geometry), blockSize);
  sls->saveBlocks = 1 + (sls->masterIndexBlocks +
                         sls->pageMapBlocks + sls->openChapterBlocks);
  sls->subIndexBlocks = sls->volumeBlocks + (sls->numSaves * sls->saveBlocks);
  sls->totalBlocks = 3 + sls->subIndexBlocks;

  return UDS_SUCCESS;
}

/*****************************************************************************/
int udsComputeIndexSize(const UdsConfiguration  config,
                        unsigned int            numCheckpoints,
                        uint64_t               *indexSize)
{
  SaveLayoutSizes sizes;
  int result = computeSizes(&sizes, config, UDS_BLOCK_SIZE, numCheckpoints);
  if (result != UDS_SUCCESS) {
    return result;
  }

  if (indexSize != NULL) {
    *indexSize = sizes.totalBlocks * sizes.blockSize;
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int openLayoutReader(IndexLayout     *layout,
                            LayoutRegion    *lr,
                            BufferedReader **readerPtr)
{
  off_t start = lr->startBlock * layout->super.blockSize;
  size_t size = lr->numBlocks * layout->super.blockSize;
  return openBufferedReader(layout->factory, start, size, readerPtr);
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int openLayoutWriter(IndexLayout     *layout,
                            LayoutRegion    *lr,
                            BufferedWriter **writerPtr)
{
  off_t start = lr->startBlock * layout->super.blockSize;
  size_t size = lr->numBlocks * layout->super.blockSize;
  return openBufferedWriter(layout->factory, start, size, writerPtr);
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int decodeIndexSaveData(Buffer *buffer, IndexSaveData *saveData)
{
  int result = getUInt64LEFromBuffer(buffer, &saveData->timestamp);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt64LEFromBuffer(buffer, &saveData->nonce);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt32LEFromBuffer(buffer, &saveData->version);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt32LEFromBuffer(buffer, &saveData->unused__);
  if (result != UDS_SUCCESS) {
    return result;
  }
  // The unused padding has to be zeroed for correct nonce calculation
  if (saveData->unused__ != 0) {
    return UDS_CORRUPT_COMPONENT;
  }
  result = ASSERT_LOG_ONLY(contentLength(buffer) == 0,
                           "%zu bytes decoded of %zu expected",
                           bufferLength(buffer), sizeof(*saveData));
  if (result != UDS_SUCCESS) {
    return UDS_CORRUPT_COMPONENT;
  }
  return result;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int decodeRegionHeader(Buffer *buffer, RegionHeader *header)
{
  int result = getUInt64LEFromBuffer(buffer, &header->magic);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt64LEFromBuffer(buffer, &header->regionBlocks);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt16LEFromBuffer(buffer, &header->type);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt16LEFromBuffer(buffer, &header->version);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt16LEFromBuffer(buffer, &header->numRegions);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt16LEFromBuffer(buffer, &header->payload);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = ASSERT_LOG_ONLY(contentLength(buffer) == 0,
                           "%zu bytes decoded of %zu expected",
                           bufferLength(buffer), sizeof(*header));
  if (result != UDS_SUCCESS) {
    return UDS_CORRUPT_COMPONENT;
  }
  return result;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int decodeLayoutRegion(Buffer *buffer, LayoutRegion *region)
{
  size_t cl1 = contentLength(buffer);

  int result = getUInt64LEFromBuffer(buffer, &region->startBlock);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt64LEFromBuffer(buffer, &region->numBlocks);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt32LEFromBuffer(buffer, &region->checksum);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt16LEFromBuffer(buffer, &region->kind);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt16LEFromBuffer(buffer, &region->instance);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = ASSERT_LOG_ONLY(cl1 - contentLength(buffer) == sizeof(*region),
                           "%zu bytes decoded, of %zu expected",
                           cl1 - contentLength(buffer), sizeof(*region));
  if (result != UDS_SUCCESS) {
    return UDS_CORRUPT_COMPONENT;
  }
  return result;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int loadRegionTable(BufferedReader *reader, RegionTable **tablePtr)
{
  Buffer *buffer;
  int result = makeBuffer(sizeof(RegionHeader), &buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = readFromBufferedReader(reader, getBufferContents(buffer),
                                  bufferLength(buffer));
  if (result != UDS_SUCCESS) {
    freeBuffer(&buffer);
    return logErrorWithStringError(result, "cannot read region table header");
  }
  result = resetBufferEnd(buffer, bufferLength(buffer));
  if (result != UDS_SUCCESS) {
    freeBuffer(&buffer);
    return result;
  }
  RegionHeader header;
  result = decodeRegionHeader(buffer, &header);
  freeBuffer(&buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }
  if (header.magic != REGION_MAGIC) {
    return UDS_NO_INDEX;
  }
  if (header.version != 1) {
    return logErrorWithStringError(UDS_UNSUPPORTED_VERSION,
                                   "unknown region table version %" PRIu16,
                                   header.version);
  }

  RegionTable *table;
  result = ALLOCATE_EXTENDED(RegionTable, header.numRegions, LayoutRegion,
                             "single file layout region table", &table);
  if (result != UDS_SUCCESS) {
    return result;
  }

  table->header = header;
  result = makeBuffer(header.numRegions * sizeof(LayoutRegion), &buffer);
  if (result != UDS_SUCCESS) {
    FREE(table);
    return result;
  }
  result = readFromBufferedReader(reader, getBufferContents(buffer),
                                  bufferLength(buffer));
  if (result != UDS_SUCCESS) {
    FREE(table);
    freeBuffer(&buffer);
    return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                   "cannot read region table layouts");
  }
  result = resetBufferEnd(buffer, bufferLength(buffer));
  if (result != UDS_SUCCESS) {
    FREE(table);
    freeBuffer(&buffer);
    return result;
  }
  unsigned int i;
  for (i = 0; i < header.numRegions; i++){
    result = decodeLayoutRegion(buffer, &table->regions[i]);
    if (result != UDS_SUCCESS) {
      FREE(table);
      freeBuffer(&buffer);
      return result;
    }
  }
  freeBuffer(&buffer);
  *tablePtr = table;
  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int decodeSuperBlockData(Buffer *buffer, SuperBlockData *super)
{
  int result = getBytesFromBuffer(buffer, 32, super->magicLabel);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getBytesFromBuffer(buffer, 32, super->nonceInfo);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt64LEFromBuffer(buffer, &super->nonce);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt32LEFromBuffer(buffer, &super->version);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt32LEFromBuffer(buffer, &super->blockSize);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt16LEFromBuffer(buffer, &super->numIndexes);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt16LEFromBuffer(buffer, &super->maxSaves);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = skipForward(buffer, 4);      // aligment
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt64LEFromBuffer(buffer, &super->openChapterBlocks);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = getUInt64LEFromBuffer(buffer, &super->pageMapBlocks);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = ASSERT_LOG_ONLY(contentLength(buffer) == 0,
                           "%zu bytes decoded of %zu expected",
                           bufferLength(buffer), sizeof(*super));
  if (result != UDS_SUCCESS) {
    return UDS_CORRUPT_COMPONENT;
  }
  return result;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int readSuperBlockData(BufferedReader *reader,
                              SuperBlockData *super,
                              size_t          savedSize)
{
  if (savedSize != sizeof(SuperBlockData)) {
    return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                   "unexpected super block data size %zu",
                                   savedSize);
  }

  if (sizeof(super->magicLabel) != SINGLE_FILE_MAGIC_1_LENGTH) {
    return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                   "super block magic label size incorrect");
  }

  Buffer *buffer;
  int result = makeBuffer(savedSize, &buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = readFromBufferedReader(reader, getBufferContents(buffer),
                                  bufferLength(buffer));
  if (result != UDS_SUCCESS) {
    freeBuffer(&buffer);
    return logErrorWithStringError(result, "cannot read region table header");
  }
  result = resetBufferEnd(buffer, bufferLength(buffer));
  if (result != UDS_SUCCESS) {
    freeBuffer(&buffer);
    return result;
  }
  result = decodeSuperBlockData(buffer, super);
  freeBuffer(&buffer);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "cannot read super block data");
  }

  if (memcmp(super->magicLabel, SINGLE_FILE_MAGIC_1,
             SINGLE_FILE_MAGIC_1_LENGTH) != 0) {
    return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                   "unknown superblock magic label");
  }

  if ((super->version < SUPER_VERSION_MINIMUM)
      || (super->version > SUPER_VERSION_MAXIMUM)) {
    return logErrorWithStringError(UDS_UNSUPPORTED_VERSION,
                                   "unknown superblock version number %"
                                   PRIu32,
                                   super->version);
  }

  // We dropped the usage of multiple subindices before we ever ran UDS code in
  // the kernel.  We do not have code that will handle multiple subindices.
  if (super->numIndexes != 1) {
    return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                   "invalid subindex count %" PRIu32,
                                   super->numIndexes);
  }

  if (generateMasterNonce(super->nonceInfo, sizeof(super->nonceInfo)) !=
      super->nonce)
  {
    return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                   "inconsistent superblock nonce");
  }

  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int allocateSingleFileParts(IndexLayout    *layout,
                                   SuperBlockData *super)
{
  int result = ALLOCATE(super->maxSaves, IndexSaveLayout, __func__,
                        &layout->index.saves);
  if (result != UDS_SUCCESS) {
    return result;
  }

  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int loadSuperBlock(IndexLayout    *layout,
                          size_t          blockSize,
                          uint64_t        firstBlock,
                          BufferedReader *reader)
{
  RegionTable *table = NULL;
  int result = loadRegionTable(reader, &table);
  if (result != UDS_SUCCESS) {
    return result;
  }

  if (table->header.type != RH_TYPE_SUPER) {
    FREE(table);
    return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                   "not a superblock region table");
  }

  SuperBlockData superBlockData;
  result = readSuperBlockData(reader, &superBlockData, table->header.payload);
  if (result != UDS_SUCCESS) {
    FREE(table);
    return logErrorWithStringError(result, "unknown superblock format");
  }

  if (superBlockData.blockSize != blockSize) {
    FREE(table);
    return logErrorWithStringError(UDS_WRONG_INDEX_CONFIG,
                                   "superblock saved blockSize %" PRIu32
                                   " differs from supplied blockSize %zu",
                                   superBlockData.blockSize, blockSize);
  }
  initializeIndexVersion(&layout->indexVersion, superBlockData.version);

  result = allocateSingleFileParts(layout, &superBlockData);
  if (result != UDS_SUCCESS) {
    FREE(table);
    return result;
  }

  result = reconstituteSingleFileLayout(layout, &superBlockData, table,
                                        firstBlock);
  FREE(table);
  return result;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int readIndexSaveData(BufferedReader  *reader,
                             IndexSaveData   *saveData,
                             size_t           savedSize,
                             Buffer         **bufferPtr)
{
  int result = UDS_SUCCESS;
  if (savedSize == 0) {
    memset(saveData, 0, sizeof(*saveData));
  } else {
    if (savedSize < sizeof(IndexSaveData)) {
      return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                     "unexpected index save data size %zu",
                                     savedSize);
    }

    Buffer *buffer;
    result = makeBuffer(sizeof(*saveData), &buffer);
    if (result != UDS_SUCCESS) {
      return result;
    }
    result = readFromBufferedReader(reader, getBufferContents(buffer),
                                    bufferLength(buffer));
    if (result != UDS_SUCCESS) {
      freeBuffer(&buffer);
      return logErrorWithStringError(result, "cannot read index save data");
    }
    result = resetBufferEnd(buffer, bufferLength(buffer));
    if (result != UDS_SUCCESS) {
      freeBuffer(&buffer);
      return result;
    }

    result = decodeIndexSaveData(buffer, saveData);
    freeBuffer(&buffer);
    if (result != UDS_SUCCESS) {
      return result;
    }

    savedSize -= sizeof(IndexSaveData);

    if (saveData->version > 1) {
      return logErrorWithStringError(UDS_UNSUPPORTED_VERSION,
                                     "unknown index save version number %"
                                     PRIu32,
                                     saveData->version);
    }

    if (savedSize > INDEX_STATE_BUFFER_SIZE) {
      return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                     "unexpected index state buffer size %zu",
                                     savedSize);
    }
  }

  Buffer *buffer = NULL;

  if (saveData->version != 0) {
    result = makeBuffer(INDEX_STATE_BUFFER_SIZE, &buffer);
    if (result != UDS_SUCCESS) {
      return result;
    }

    if (savedSize > 0) {
      result = readFromBufferedReader(reader, getBufferContents(buffer),
                                      savedSize);
      if (result != UDS_SUCCESS) {
        freeBuffer(&buffer);
        return result;
      }
      result = resetBufferEnd(buffer, savedSize);
      if (result != UDS_SUCCESS) {
        freeBuffer(&buffer);
        return result;
      }
    }
  }

  *bufferPtr = buffer;
  return UDS_SUCCESS;
}

/*****************************************************************************/

typedef struct {
  LayoutRegion  *nextRegion;
  LayoutRegion  *lastRegion;
  uint64_t       nextBlock;
  int            result;
} RegionIterator;

/*****************************************************************************/
__attribute__((format(printf, 2, 3)))
static void iterError(RegionIterator *iter, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  int r = vLogWithStringError(LOG_ERR, UDS_UNEXPECTED_RESULT, fmt, args);
  va_end(args);
  if (iter->result == UDS_SUCCESS) {
    iter->result = r;
  }
}

/**
 * Set the next layout region in the layout according to a region table
 * iterator, unless the iterator already contains an error
 *
 * @param expect        whether to record an error or return false
 * @param lr            the layout region field to set
 * @param iter          the region iterator, which also holds the cumulative
 *                        result
 * @param numBlocks     if non-zero, the expected number of blocks
 * @param kind          the expected kind of the region
 * @param instance      the expected instance number of the region
 *
 * @return true if we meet expectations, false if we do not
 **/
static bool expectLayout(bool            expect,
                         LayoutRegion   *lr,
                         RegionIterator *iter,
                         uint64_t        numBlocks,
                         RegionKind      kind,
                         unsigned int    instance)
{
  if (iter->result != UDS_SUCCESS) {
    return false;
  }

  if (iter->nextRegion == iter->lastRegion) {
    if (expect) {
      iterError(iter, "ran out of layout regions in region table");
    }
    return false;
  }

  if (iter->nextRegion->startBlock != iter->nextBlock) {
    iterError(iter, "layout region not at expected offset");
    return false;
  }

  if (iter->nextRegion->kind != kind) {
    if (expect) {
      iterError(iter, "layout region has incorrect kind");
    }
    return false;
  }

  if (iter->nextRegion->instance != instance) {
    iterError(iter, "layout region has incorrect instance");
    return false;
  }

  if (numBlocks > 0 && iter->nextRegion->numBlocks != numBlocks) {
    iterError(iter, "layout region size is incorrect");
    return false;
  }

  if (lr != NULL) {
    *lr = *iter->nextRegion;
  }

  iter->nextBlock += iter->nextRegion->numBlocks;
  iter->nextRegion++;
  return true;
}

/*****************************************************************************/
static void setupLayout(LayoutRegion *lr,
                        uint64_t     *nextAddrPtr,
                        uint64_t      regionSize,
                        unsigned int  kind,
                        unsigned int  instance)
{
  *lr = (LayoutRegion) {
    .startBlock = *nextAddrPtr,
    .numBlocks  = regionSize,
    .checksum   = 0,
    .kind       = kind,
    .instance   = instance,
  };
  *nextAddrPtr += regionSize;
}

/*****************************************************************************/
static void populateIndexSaveLayout(IndexSaveLayout *isl,
                                    SuperBlockData  *super,
                                    unsigned int     numZones,
                                    IndexSaveType    saveType)
{
  uint64_t nextBlock = isl->indexSave.startBlock;

  setupLayout(&isl->header, &nextBlock, 1, RL_KIND_HEADER, RL_SOLE_INSTANCE);
  setupLayout(&isl->indexPageMap, &nextBlock, super->pageMapBlocks,
              RL_KIND_INDEX_PAGE_MAP, RL_SOLE_INSTANCE);

  uint64_t blocksAvail = (isl->indexSave.numBlocks -
                          (nextBlock - isl->indexSave.startBlock) -
                          super->openChapterBlocks);

  if (numZones > 0) {
    uint64_t miBlockCount = blocksAvail / numZones;
    unsigned int z;
    for (z = 0; z < numZones; ++z) {
      LayoutRegion *miz = &isl->masterIndexZones[z];
      setupLayout(miz, &nextBlock, miBlockCount, RL_KIND_MASTER_INDEX, z);
    }
  }
  if (saveType == IS_SAVE && isl->openChapter != NULL) {
    setupLayout(isl->openChapter, &nextBlock, super->openChapterBlocks,
                RL_KIND_OPEN_CHAPTER, RL_SOLE_INSTANCE);
  }
  setupLayout(&isl->freeSpace, &nextBlock,
              (isl->indexSave.numBlocks -
               (nextBlock - isl->indexSave.startBlock)),
               RL_KIND_SCRATCH, RL_SOLE_INSTANCE);
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int reconstructIndexSave(IndexSaveLayout *isl,
                                IndexSaveData   *saveData,
                                SuperBlockData  *super,
                                RegionTable     *table)
{
  isl->numZones = 0;
  isl->saveData = *saveData;
  isl->read     = false;
  isl->written  = false;

  if (table->header.type == RH_TYPE_SAVE) {
    isl->saveType = IS_SAVE;
  } else if (table->header.type == RH_TYPE_CHECKPOINT) {
    isl->saveType = IS_CHECKPOINT;
  } else {
    isl->saveType = NO_SAVE;
  }

  if ((table->header.numRegions == 0) ||
      ((table->header.numRegions == 1) &&
       (table->regions[0].kind == RL_KIND_SCRATCH)))
  {
    populateIndexSaveLayout(isl, super, 0, NO_SAVE);
    return UDS_SUCCESS;
  }

  RegionIterator iter = {
    .nextRegion = table->regions,
    .lastRegion = table->regions + table->header.numRegions,
    .nextBlock  = isl->indexSave.startBlock,
    .result     = UDS_SUCCESS,
  };

  expectLayout(true, &isl->header, &iter, 1, RL_KIND_HEADER, RL_SOLE_INSTANCE);
  expectLayout(true, &isl->indexPageMap, &iter, 0,
               RL_KIND_INDEX_PAGE_MAP, RL_SOLE_INSTANCE);
  unsigned int n = 0;
  RegionIterator tmpIter;
  for (tmpIter = iter;
       expectLayout(false, NULL, &tmpIter, 0, RL_KIND_MASTER_INDEX, n);
       ++n)
    ;
  isl->numZones = n;

  int result = UDS_SUCCESS;

  if (isl->numZones > 0) {
    result = ALLOCATE(n, LayoutRegion, "master index layout regions",
                      &isl->masterIndexZones);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  if (isl->saveType == IS_SAVE) {
    result = ALLOCATE(1, LayoutRegion, "open chapter layout region",
                      &isl->openChapter);
    if (result != UDS_SUCCESS) {
      FREE(isl->masterIndexZones);
      return result;
    }
  }

  unsigned int z;
  for (z = 0; z < isl->numZones; ++z) {
    expectLayout(true, &isl->masterIndexZones[z], &iter, 0,
                 RL_KIND_MASTER_INDEX, z);
  }
  if (isl->saveType == IS_SAVE) {
    expectLayout(true, isl->openChapter, &iter, 0,
                 RL_KIND_OPEN_CHAPTER, RL_SOLE_INSTANCE);
  }
  if (!expectLayout(false, &isl->freeSpace, &iter, 0,
                    RL_KIND_SCRATCH, RL_SOLE_INSTANCE))
  {
    isl->freeSpace = (LayoutRegion) {
      .startBlock = iter.nextBlock,
      .numBlocks  = (isl->indexSave.startBlock +
                     isl->indexSave.numBlocks) - iter.nextBlock,
      .checksum   = 0,
      .kind       = RL_KIND_SCRATCH,
      .instance   = RL_SOLE_INSTANCE,
    };
    iter.nextBlock = isl->freeSpace.startBlock + isl->freeSpace.numBlocks;
  }

  if (iter.result != UDS_SUCCESS) {
    return iter.result;
  }
  if (iter.nextRegion != iter.lastRegion) {
    return logErrorWithStringError(UDS_UNEXPECTED_RESULT,
                                   "expected %ld additional regions",
                                   iter.lastRegion - iter.nextRegion);
  }
  if (iter.nextBlock != isl->indexSave.startBlock + isl->indexSave.numBlocks) {
    return logErrorWithStringError(UDS_UNEXPECTED_RESULT,
                                   "index save layout table incomplete");
  }

  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int loadIndexSave(IndexSaveLayout *isl,
                         SuperBlockData  *super,
                         BufferedReader  *reader,
                         unsigned int     saveId)
{
  RegionTable *table = NULL;
  int result = loadRegionTable(reader, &table);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result,
                                   "cannot read index 0 save %u header",
                                   saveId);
  }

  if (table->header.regionBlocks != isl->indexSave.numBlocks) {
    uint64_t regionBlocks = table->header.regionBlocks;
    FREE(table);
    return logErrorWithStringError(UDS_CORRUPT_COMPONENT,
                                   "unexpected index 0 save %u "
                                   "region block count %" PRIu64,
                                   saveId, regionBlocks);
  }

  if (table->header.type != RH_TYPE_SAVE &&
      table->header.type != RH_TYPE_CHECKPOINT &&
      table->header.type != RH_TYPE_UNSAVED)
  {
    unsigned int type = table->header.type;
    FREE(table);
    return logErrorWithStringError(UDS_CORRUPT_COMPONENT, "unexpected"
                                   " index 0 save %u header type %u",
                                   saveId, type);
  }

  IndexSaveData indexSaveData;
  result = readIndexSaveData(reader, &indexSaveData, table->header.payload,
                             &isl->indexStateBuffer);
  if (result != UDS_SUCCESS) {
    FREE(table);
    return logErrorWithStringError(result,
                                   "unknown index 0 save %u data format",
                                   saveId);
  }

  result = reconstructIndexSave(isl, &indexSaveData, super, table);
  FREE(table);

  if (result != UDS_SUCCESS) {
    freeBuffer(&isl->indexStateBuffer);
    return logErrorWithStringError(result,
                                   "cannot reconstruct index 0 save %u",
                                   saveId);
  }
  isl->read = true;
  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int loadSubIndexRegions(IndexLayout *layout)
{
  unsigned int j;
  for (j = 0; j < layout->super.maxSaves; ++j) {
    IndexSaveLayout *isl = &layout->index.saves[j];

    BufferedReader *reader;
    int result = openLayoutReader(layout, &isl->indexSave, &reader);
    if (result != UDS_SUCCESS) {
      logErrorWithStringError(result, "cannot get reader for index 0 save %u",
                              j);
      while (j-- > 0) {
        IndexSaveLayout *isl = &layout->index.saves[j];
        FREE(isl->masterIndexZones);
        FREE(isl->openChapter);
        freeBuffer(&isl->indexStateBuffer);
      }
      return result;
    }

    result = loadIndexSave(isl, &layout->super, reader, j);
    freeBufferedReader(reader);
    if (result != UDS_SUCCESS) {
      while (j-- > 0) {
        IndexSaveLayout *isl = &layout->index.saves[j];
        FREE(isl->masterIndexZones);
        FREE(isl->openChapter);
        freeBuffer(&isl->indexStateBuffer);
      }
      return result;
    }
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int loadIndexLayout(IndexLayout *layout)
{
  BufferedReader *reader;
  int result = openBufferedReader(layout->factory, layout->offset,
                                  UDS_BLOCK_SIZE, &reader);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "unable to read superblock");
  }

  result = loadSuperBlock(layout, UDS_BLOCK_SIZE,
                          layout->offset / UDS_BLOCK_SIZE, reader);
  freeBufferedReader(reader);
  if (result != UDS_SUCCESS) {
    FREE(layout->index.saves);
    layout->index.saves = NULL;
    return result;
  }

  result = loadSubIndexRegions(layout);
  if (result != UDS_SUCCESS) {
    FREE(layout->index.saves);
    layout->index.saves = NULL;
    return result;
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
static void generateSuperBlockData(size_t          blockSize,
                                   unsigned int    maxSaves,
                                   uint64_t        openChapterBlocks,
                                   uint64_t        pageMapBlocks,
                                   SuperBlockData *super)
{
  memset(super, 0, sizeof(*super));
  memcpy(super->magicLabel, SINGLE_FILE_MAGIC_1, SINGLE_FILE_MAGIC_1_LENGTH);
  createUniqueNonceData(super->nonceInfo, sizeof(super->nonceInfo));

  super->nonce             = generateMasterNonce(super->nonceInfo,
                                                 sizeof(super->nonceInfo));
  super->version           = SUPER_VERSION_CURRENT;
  super->blockSize         = blockSize;
  super->numIndexes        = 1;
  super->maxSaves          = maxSaves;
  super->openChapterBlocks = openChapterBlocks;
  super->pageMapBlocks     = pageMapBlocks;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int resetIndexSaveLayout(IndexSaveLayout *isl,
                                uint64_t        *nextBlockPtr,
                                uint64_t         saveBlocks,
                                uint64_t         pageMapBlocks,
                                unsigned int     instance)
{
  uint64_t startBlock = *nextBlockPtr;

  if (isl->masterIndexZones) {
    FREE(isl->masterIndexZones);
  }
  if (isl->openChapter) {
    FREE(isl->openChapter);
  }
  if (isl->indexStateBuffer) {
    freeBuffer(&isl->indexStateBuffer);
  }
  memset(isl, 0, sizeof(*isl));
  isl->saveType = NO_SAVE;
  setupLayout(&isl->indexSave, &startBlock, saveBlocks, RL_KIND_SAVE,
              instance);
  setupLayout(&isl->header, nextBlockPtr,  1, RL_KIND_HEADER,
              RL_SOLE_INSTANCE);
  setupLayout(&isl->indexPageMap, nextBlockPtr, pageMapBlocks,
              RL_KIND_INDEX_PAGE_MAP, RL_SOLE_INSTANCE);
  uint64_t remaining = startBlock - *nextBlockPtr;
  setupLayout(&isl->freeSpace, nextBlockPtr, remaining, RL_KIND_SCRATCH,
              RL_SOLE_INSTANCE);
  // number of zones is a save-time parameter
  // presence of open chapter is a save-time parameter
  return UDS_SUCCESS;
}

/*****************************************************************************/
static void defineSubIndexNonce(SubIndexLayout *sil,
                                uint64_t        masterNonce,
                                unsigned int    indexId)
{
  struct subIndexNonceData {
    uint64_t offset;
    uint16_t indexId;
  };
  byte buffer[sizeof(struct subIndexNonceData)] = { 0 };
  size_t offset = 0;
  encodeUInt64LE(buffer, &offset, sil->subIndex.startBlock);
  encodeUInt16LE(buffer, &offset, indexId);
  sil->nonce = generateSecondaryNonce(masterNonce, buffer, sizeof(buffer));
  if (sil->nonce == 0) {
    sil->nonce = generateSecondaryNonce(~masterNonce + 1,
                                        buffer, sizeof(buffer));
  }
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int setupSubIndex(SubIndexLayout  *sil,
                         uint64_t        *nextBlockPtr,
                         SaveLayoutSizes *sls,
                         unsigned int     instance,
                         uint64_t         masterNonce)
{
  uint64_t startBlock = *nextBlockPtr;

  setupLayout(&sil->subIndex, &startBlock, sls->subIndexBlocks,
              RL_KIND_INDEX, instance);
  setupLayout(&sil->volume, nextBlockPtr, sls->volumeBlocks,
              RL_KIND_VOLUME, RL_SOLE_INSTANCE);
  unsigned int i;
  for (i = 0; i < sls->numSaves; ++i) {
    int result = resetIndexSaveLayout(&sil->saves[i], nextBlockPtr,
                                      sls->saveBlocks, sls->pageMapBlocks, i);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  if (startBlock != *nextBlockPtr) {
    return logErrorWithStringError(UDS_UNEXPECTED_RESULT,
                                   "sub index layout regions don't agree");
  }

  defineSubIndexNonce(sil, masterNonce, instance);
  return UDS_SUCCESS;
}

/*****************************************************************************/
/**
 * Initialize a single file layout using the save layout sizes specified.
 *
 * @param layout  the layout to initialize
 * @param offset  the offset in bytes from the start of the backing storage
 * @param size    the size in bytes of the backing storage
 * @param sls     a populated SaveLayoutSizes object
 *
 * @return UDS_SUCCESS or an error code, potentially
 *         UDS_INSUFFICIENT_INDEX_SPACE if the size of the backing store
 *              is not sufficient for the index configuration,
 *         UDS_BAD_INDEX_ALIGNMENT if the offset specified does not
 *              align properly with the index block and page sizes]
 *         various other errors
 **/
__attribute__((warn_unused_result))
static int initSingleFileLayout(IndexLayout     *layout,
                                uint64_t         offset,
                                uint64_t         size,
                                SaveLayoutSizes *sls)
{
  layout->totalBlocks = sls->totalBlocks;

  if (size < sls->totalBlocks * sls->blockSize) {
    return logErrorWithStringError(UDS_INSUFFICIENT_INDEX_SPACE,
                                   "not enough space for index as configured");
  }

  generateSuperBlockData(sls->blockSize, sls->numSaves, sls->openChapterBlocks,
                         sls->pageMapBlocks, &layout->super);
  initializeIndexVersion(&layout->indexVersion, SUPER_VERSION_CURRENT);

  int result = allocateSingleFileParts(layout, &layout->super);
  if (result != UDS_SUCCESS) {
    return result;
  }

  uint64_t nextBlock = offset / sls->blockSize;

  setupLayout(&layout->header, &nextBlock, 1, RL_KIND_HEADER,
              RL_SOLE_INSTANCE);
  setupLayout(&layout->config, &nextBlock, 1, RL_KIND_CONFIG,
              RL_SOLE_INSTANCE);
  result = setupSubIndex(&layout->index, &nextBlock, sls, 0,
                         layout->super.nonce);
  if (result != UDS_SUCCESS) {
    return result;
  }
  setupLayout(&layout->seal, &nextBlock, 1, RL_KIND_SEAL, RL_SOLE_INSTANCE);
  if (nextBlock * sls->blockSize > offset + size) {
    return logErrorWithStringError(UDS_UNEXPECTED_RESULT,
                                   "layout does not fit as expected");
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
static void expectSubIndex(SubIndexLayout *sil,
                           RegionIterator *iter,
                           SuperBlockData *super,
                           unsigned int    instance)
{
  if (iter->result != UDS_SUCCESS) {
    return;
  }

  uint64_t startBlock = iter->nextBlock;

  expectLayout(true, &sil->subIndex, iter, 0, RL_KIND_INDEX, instance);

  uint64_t endBlock = iter->nextBlock;
  iter->nextBlock = startBlock;

  expectLayout(true, &sil->volume, iter, 0, RL_KIND_VOLUME, RL_SOLE_INSTANCE);

  unsigned int i;
  for (i = 0; i < super->maxSaves; ++i) {
    IndexSaveLayout *isl = &sil->saves[i];
    expectLayout(true, &isl->indexSave, iter, 0, RL_KIND_SAVE, i);
  }

  if (iter->nextBlock != endBlock) {
    iterError(iter, "sub index region does not span all saves");
  }

  defineSubIndexNonce(sil, super->nonce, instance);
}

/*****************************************************************************/

/**
 * Initialize a single file layout from the region table and super block data
 * stored in stable storage.
 *
 * @param layout      the layout to initialize
 * @param region      the IO region for this layout
 * @param super       the super block data read from the superblock
 * @param table       the region table read from the superblock
 * @param firstBlock  the first block number in the region
 *
 * @return UDS_SUCCESS or an error code
 **/
__attribute__((warn_unused_result))
static int reconstituteSingleFileLayout(IndexLayout    *layout,
                                        SuperBlockData *super,
                                        RegionTable    *table,
                                        uint64_t        firstBlock)
{
  layout->super       = *super;
  layout->totalBlocks = table->header.regionBlocks;

  RegionIterator iter = {
    .nextRegion = table->regions,
    .lastRegion = table->regions + table->header.numRegions,
    .nextBlock  = firstBlock,
    .result     = UDS_SUCCESS
  };

  expectLayout(true, &layout->header, &iter, 1, RL_KIND_HEADER,
               RL_SOLE_INSTANCE);
  expectLayout(true, &layout->config, &iter, 1, RL_KIND_CONFIG,
               RL_SOLE_INSTANCE);
  expectSubIndex(&layout->index, &iter, &layout->super, 0);
  expectLayout(true, &layout->seal, &iter, 1, RL_KIND_SEAL, RL_SOLE_INSTANCE);

  if (iter.result != UDS_SUCCESS) {
    return iter.result;
  }

  if (iter.nextBlock != firstBlock + layout->totalBlocks) {
    return logErrorWithStringError(UDS_UNEXPECTED_RESULT,
                                   "layout table does not span total blocks");
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int saveSubIndexRegions(IndexLayout *layout)
{
  SubIndexLayout *sil = &layout->index;
  unsigned int j;
  for (j = 0; j < layout->super.maxSaves; ++j) {
    IndexSaveLayout *isl = &sil->saves[j];
    int result = writeIndexSaveLayout(layout, isl);
    if (result != UDS_SUCCESS) {
      return logErrorWithStringError(result,
                                     "unable to format index %u save 0 layout",
                                     j);
    }
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int makeSingleFileRegionTable(IndexLayout   *layout,
                                     unsigned int  *numRegionsPtr,
                                     RegionTable  **tablePtr)
{
  unsigned int numRegions =
    1 +                      // header
    1 +                      // config
    1 +                      // index
    1 +                      // volume
    layout->super.maxSaves + // saves
    1;                       // seal

  RegionTable *table;
  int result = ALLOCATE_EXTENDED(RegionTable, numRegions, LayoutRegion,
                                 "layout region table", &table);
  if (result != UDS_SUCCESS) {
    return result;
  }

  LayoutRegion *lr = &table->regions[0];
  *lr++ = layout->header;
  *lr++ = layout->config;
  SubIndexLayout *sil = &layout->index;
  *lr++ = sil->subIndex;
  *lr++ = sil->volume;
  unsigned int j;
  for (j = 0; j < layout->super.maxSaves; ++j) {
    *lr++ = sil->saves[j].indexSave;
  }
  *lr++ = layout->seal;

  result = ASSERT((lr == &table->regions[numRegions]),
                  "incorrect number of regions");
  if (result != UDS_SUCCESS) {
    return result;
  }

  *numRegionsPtr = numRegions;
  *tablePtr      = table;
  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int encodeIndexSaveData(Buffer *buffer, IndexSaveData *saveData)
{
  int result = putUInt64LEIntoBuffer(buffer, saveData->timestamp);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt64LEIntoBuffer(buffer, saveData->nonce);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt32LEIntoBuffer(buffer, saveData->version);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = zeroBytes(buffer, 4);        /* padding */
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = ASSERT_LOG_ONLY(contentLength(buffer) == sizeof *saveData,
                           "%zu bytes encoded of %zu expected",
                           contentLength(buffer), sizeof(*saveData));
  return result;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int encodeRegionHeader(Buffer *buffer, RegionHeader *header)
{
  size_t startingLength = contentLength(buffer);
  int result = putUInt64LEIntoBuffer(buffer, REGION_MAGIC);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt64LEIntoBuffer(buffer, header->regionBlocks);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt16LEIntoBuffer(buffer, header->type);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt16LEIntoBuffer(buffer, header->version);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt16LEIntoBuffer(buffer, header->numRegions);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt16LEIntoBuffer(buffer, header->payload);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result
    = ASSERT_LOG_ONLY(contentLength(buffer) - startingLength == sizeof(*header),
                      "%zu bytes encoded, of %zu expected",
                      contentLength(buffer) - startingLength, sizeof(*header));
  return result;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int encodeLayoutRegion(Buffer *buffer, LayoutRegion *region)
{
  size_t startingLength = contentLength(buffer);
  int result = putUInt64LEIntoBuffer(buffer, region->startBlock);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt64LEIntoBuffer(buffer, region->numBlocks);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt32LEIntoBuffer(buffer, region->checksum);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt16LEIntoBuffer(buffer, region->kind);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt16LEIntoBuffer(buffer, region->instance);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result
    = ASSERT_LOG_ONLY(contentLength(buffer) - startingLength == sizeof(*region),
                      "%zu bytes encoded, of %zu expected",
                      contentLength(buffer) - startingLength, sizeof(*region));
  return result;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int encodeSuperBlockData(Buffer *buffer, SuperBlockData *super)
{
  int result = putBytes(buffer, 32, &super->magicLabel);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putBytes(buffer, 32, &super->nonceInfo);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt64LEIntoBuffer(buffer, super->nonce);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt32LEIntoBuffer(buffer, super->version);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt32LEIntoBuffer(buffer, super->blockSize);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt16LEIntoBuffer(buffer, super->numIndexes);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt16LEIntoBuffer(buffer, super->maxSaves);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = zeroBytes(buffer, 4);      // aligment
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt64LEIntoBuffer(buffer, super->openChapterBlocks);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = putUInt64LEIntoBuffer(buffer, super->pageMapBlocks);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = ASSERT_LOG_ONLY(contentLength(buffer) == sizeof(SuperBlockData),
                           "%zu bytes encoded, of %zu expected",
                           contentLength(buffer), sizeof(SuperBlockData));
  return result;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int writeSingleFileHeader(IndexLayout    *layout,
                                 RegionTable    *table,
                                 unsigned int    numRegions,
                                 BufferedWriter *writer)
{
  table->header = (RegionHeader) {
    .magic        = REGION_MAGIC,
    .regionBlocks = layout->totalBlocks,
    .type         = RH_TYPE_SUPER,
    .version      = 1,
    .numRegions   = numRegions,
    .payload      = sizeof(layout->super),
  };

  size_t tableSize = sizeof(RegionTable) + numRegions * sizeof(LayoutRegion);

  Buffer *buffer;
  int result = makeBuffer(tableSize, &buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = encodeRegionHeader(buffer, &table->header);

  unsigned int i;
  for (i = 0; i < numRegions; i++) {
    if (result == UDS_SUCCESS) {
      result = encodeLayoutRegion(buffer, &table->regions[i]);
    }
  }

  if (result == UDS_SUCCESS) {
    result = writeToBufferedWriter(writer,  getBufferContents(buffer),
                                   contentLength(buffer));
  }
  freeBuffer(&buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = makeBuffer(sizeof(layout->super), &buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = encodeSuperBlockData(buffer, &layout->super);
  if (result != UDS_SUCCESS) {
    freeBuffer(&buffer);
    return result;
  }

  result = writeToBufferedWriter(writer,  getBufferContents(buffer),
                                 contentLength(buffer));
  freeBuffer(&buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }
  return flushBufferedWriter(writer);
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int saveSingleFileConfiguration(IndexLayout *layout)
{
  int result = saveSubIndexRegions(layout);
  if (result != UDS_SUCCESS) {
    return result;
  }

  RegionTable  *table;
  unsigned int  numRegions;
  result = makeSingleFileRegionTable(layout, &numRegions, &table);
  if (result != UDS_SUCCESS) {
    return result;
  }

  BufferedWriter *writer = NULL;
  result = openLayoutWriter(layout, &layout->header, &writer);
  if (result != UDS_SUCCESS) {
    FREE(table);
    return result;
  }

  result = writeSingleFileHeader(layout, table, numRegions, writer);
  FREE(table);
  freeBufferedWriter(writer);

  return result;
}

/*****************************************************************************/
void putIndexLayout(IndexLayout **layoutPtr)
{
  if (layoutPtr == NULL) {
    return;
  }
  IndexLayout *layout = *layoutPtr;
  *layoutPtr = NULL;
  if ((layout == NULL) || (--layout->refCount > 0)) {
    return;
  }

  SubIndexLayout *sil = &layout->index;
  if (sil->saves != NULL) {
    unsigned int j;
    for (j = 0; j < layout->super.maxSaves; ++j) {
      IndexSaveLayout *isl = &sil->saves[j];
      FREE(isl->masterIndexZones);
      FREE(isl->openChapter);
      freeBuffer(&isl->indexStateBuffer);
    }
  }
  FREE(sil->saves);

  if (layout->factory != NULL) {
    putIOFactory(layout->factory);
  }
  FREE(layout);
}

/*****************************************************************************/
void getIndexLayout(IndexLayout *layout, IndexLayout **layoutPtr)
{
  ++layout->refCount;
  *layoutPtr = layout;
}

/*****************************************************************************/
const struct index_version *getIndexVersion(IndexLayout *layout)
{
  return &layout->indexVersion;
}

/*****************************************************************************/
int writeIndexConfig(IndexLayout *layout, UdsConfiguration config)
{
  BufferedWriter *writer = NULL;
  int result = openLayoutWriter(layout, &layout->config, &writer);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "failed to open config region");
  }

  result = writeConfigContents(writer, config);
  if (result != UDS_SUCCESS) {
    freeBufferedWriter(writer);
    return logErrorWithStringError(result, "failed to write config region");
  }
  result = flushBufferedWriter(writer);
  if (result != UDS_SUCCESS) {
    freeBufferedWriter(writer);
    return logErrorWithStringError(result, "cannot flush config writer");
  }
  freeBufferedWriter(writer);
  return UDS_SUCCESS;
}

/*****************************************************************************/
int verifyIndexConfig(IndexLayout *layout, UdsConfiguration config)
{
  BufferedReader *reader = NULL;
  int result = openLayoutReader(layout, &layout->config, &reader);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "failed to open config reader");
  }

  struct udsConfiguration storedConfig;
  result = readConfigContents(reader, &storedConfig);
  if (result != UDS_SUCCESS) {
    freeBufferedReader(reader);
    return logErrorWithStringError(result, "failed to read config region");
  }
  freeBufferedReader(reader);

  return (areUdsConfigurationsEqual(&storedConfig, config)
          ? UDS_SUCCESS
          : UDS_NO_INDEX);
}

#ifdef __KERNEL__
/*****************************************************************************/
int openVolumeBufio(IndexLayout             *layout,
                    size_t                   blockSize,
                    unsigned int             reservedBuffers,
                    struct dm_bufio_client **clientPtr)
{
  off_t offset = layout->index.volume.startBlock * layout->super.blockSize;
  return makeBufio(layout->factory, offset, blockSize, reservedBuffers,
                   clientPtr);
}
#else
/*****************************************************************************/
int openVolumeRegion(IndexLayout *layout, IORegion **regionPtr)
{
  LayoutRegion *lr = &layout->index.volume;
  off_t start = lr->startBlock * layout->super.blockSize;
  size_t size = lr->numBlocks * layout->super.blockSize;
  int result =  makeIORegion(layout->factory, start, size, regionPtr);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result,
                                   "cannot access index volume region");
  }
  return UDS_SUCCESS;
}
#endif

/*****************************************************************************/
uint64_t getVolumeNonce(IndexLayout *layout)
{
  return layout->index.nonce;
}

/*****************************************************************************/
static uint64_t generateIndexSaveNonce(uint64_t         volumeNonce,
                                       IndexSaveLayout *isl)
{
  struct SaveNonceData {
    IndexSaveData data;
    uint64_t      offset;
  } nonceData;

  nonceData.data = isl->saveData;
  nonceData.data.nonce = 0;
  nonceData.offset = isl->indexSave.startBlock;

  byte buffer[sizeof(nonceData)];
  size_t offset = 0;
  encodeUInt64LE(buffer, &offset, nonceData.data.timestamp);
  encodeUInt64LE(buffer, &offset, nonceData.data.nonce);
  encodeUInt32LE(buffer, &offset, nonceData.data.version);
  encodeUInt32LE(buffer, &offset, 0U);    // padding
  encodeUInt64LE(buffer, &offset, nonceData.offset);
  ASSERT_LOG_ONLY(offset == sizeof(nonceData),
                  "%zu bytes encoded of %zu expected",
                  offset, sizeof(nonceData));
  return generateSecondaryNonce(volumeNonce, buffer, sizeof(buffer));
}

/*****************************************************************************/
static int validateIndexSaveLayout(IndexSaveLayout *isl,
                                   uint64_t         volumeNonce,
                                   uint64_t        *saveTimePtr)
{
  if (isl->saveType == NO_SAVE || isl->numZones == 0 ||
      isl->saveData.timestamp == 0)
  {
    return UDS_BAD_STATE;
  }
  if (isl->saveData.nonce != generateIndexSaveNonce(volumeNonce, isl)) {
    return UDS_BAD_STATE;
  }
  if (saveTimePtr != NULL) {
    *saveTimePtr = isl->saveData.timestamp;
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int selectOldestIndexSaveLayout(SubIndexLayout   *sil,
                                       unsigned int      maxSaves,
                                       IndexSaveLayout **islPtr)
{
  IndexSaveLayout *oldest = NULL;
  uint64_t         oldestTime = 0;

  // find the oldest valid or first invalid slot
  IndexSaveLayout *isl;
  for (isl = sil->saves; isl < sil->saves + maxSaves; ++isl) {
    uint64_t saveTime = 0;
    int result = validateIndexSaveLayout(isl, sil->nonce, &saveTime);
    if (result != UDS_SUCCESS) {
      saveTime = 0;
    }
    if (oldest == NULL || saveTime < oldestTime) {
      oldest = isl;
      oldestTime = saveTime;
    }
  }

  int result = ASSERT((oldest != NULL), "no oldest or free save slot");
  if (result != UDS_SUCCESS) {
    return result;
  }
  *islPtr = oldest;
  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int selectLatestIndexSaveLayout(SubIndexLayout   *sil,
                                       unsigned int      maxSaves,
                                       IndexSaveLayout **islPtr)
{
  IndexSaveLayout *latest = NULL;
  uint64_t         latestTime = 0;

  // find the latest valid save slot
  IndexSaveLayout *isl;
  for (isl = sil->saves; isl < sil->saves + maxSaves; ++isl) {
    uint64_t saveTime = 0;
    int result = validateIndexSaveLayout(isl, sil->nonce, &saveTime);
    if (result != UDS_SUCCESS) {
      continue;
    }
    if (saveTime > latestTime) {
      latest = isl;
      latestTime = saveTime;
    }
  }

  if (latest == NULL) {
    return UDS_INDEX_NOT_SAVED_CLEANLY;
  }
  *islPtr = latest;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static uint64_t getTimeMS(AbsTime time)
{
  time_t t = asTimeT(time);
  RelTime r = timeDifference(time, fromTimeT(t));
  return (uint64_t) t * 1000 + relTimeToMilliseconds(r);
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int instantiateIndexSaveLayout(IndexSaveLayout *isl,
                                      SuperBlockData  *super,
                                      uint64_t         volumeNonce,
                                      unsigned int     numZones,
                                      IndexSaveType    saveType)
{
  int result = UDS_SUCCESS;
  if (isl->openChapter && saveType == IS_CHECKPOINT) {
    FREE(isl->openChapter);
    isl->openChapter = NULL;
  } else if (isl->openChapter == NULL && saveType == IS_SAVE) {
    result = ALLOCATE(1, LayoutRegion, "open chapter layout",
                      &isl->openChapter);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }
  if (numZones != isl->numZones) {
    if (isl->masterIndexZones != NULL) {
      FREE(isl->masterIndexZones);
    }
    result = ALLOCATE(numZones, LayoutRegion, "master index zone layouts",
                      &isl->masterIndexZones);
    if (result != UDS_SUCCESS) {
      return result;
    }
    isl->numZones = numZones;
  }

  populateIndexSaveLayout(isl, super, numZones, saveType);

  result = makeBuffer(INDEX_STATE_BUFFER_SIZE, &isl->indexStateBuffer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  isl->read = isl->written = false;
  isl->saveType = saveType;
  memset(&isl->saveData, 0, sizeof(isl->saveData));
  isl->saveData.timestamp = getTimeMS(currentTime(CLOCK_REALTIME));
  isl->saveData.version   = 1;

  isl->saveData.nonce = generateIndexSaveNonce(volumeNonce, isl);

  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int invalidateOldSave(IndexLayout *layout, IndexSaveLayout *isl)
{
  uint64_t startBlock = isl->indexSave.startBlock;
  uint64_t saveBlocks = isl->indexSave.numBlocks;
  unsigned int save   = isl->indexSave.instance;

  int result = resetIndexSaveLayout(isl, &startBlock, saveBlocks,
                                    layout->super.pageMapBlocks, save);
  if (result != UDS_SUCCESS) {
    return result;
  }

  return writeIndexSaveLayout(layout, isl);
}

/*****************************************************************************/
int setupIndexSaveSlot(IndexLayout   *layout,
                       unsigned int   numZones,
                       IndexSaveType  saveType,
                       unsigned int  *saveSlotPtr)
{
  SubIndexLayout *sil = &layout->index;

  IndexSaveLayout *isl = NULL;
  int result = selectOldestIndexSaveLayout(sil, layout->super.maxSaves, &isl);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = invalidateOldSave(layout, isl);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = instantiateIndexSaveLayout(isl, &layout->super, sil->nonce,
                                      numZones, saveType);
  if (result != UDS_SUCCESS) {
    return result;
  }

  *saveSlotPtr = isl - sil->saves;
  return UDS_SUCCESS;
}

/*****************************************************************************/
int findLatestIndexSaveSlot(IndexLayout  *layout,
                            unsigned int *numZonesPtr,
                            unsigned int *slotPtr)
{
  SubIndexLayout *sil = &layout->index;

  IndexSaveLayout *isl = NULL;
  int result = selectLatestIndexSaveLayout(sil, layout->super.maxSaves, &isl);
  if (result != UDS_SUCCESS) {
    return result;
  }

  if (numZonesPtr != NULL) {
    *numZonesPtr = isl->numZones;
  }
  if (slotPtr != NULL) {
    *slotPtr = isl - sil->saves;
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int makeIndexSaveRegionTable(IndexSaveLayout  *isl,
                                    unsigned int     *numRegionsPtr,
                                    RegionTable     **tablePtr)
{
  unsigned int numRegions =
    1 +                         // header
    1 +                         // index page map
    isl->numZones +             // master index zones
    (bool) isl->openChapter;    // open chapter if needed

  if (isl->freeSpace.numBlocks > 0) {
    numRegions++;
  }

  RegionTable *table;
  int result = ALLOCATE_EXTENDED(RegionTable, numRegions, LayoutRegion,
                                 "layout region table for ISL", &table);
  if (result != UDS_SUCCESS) {
    return result;
  }

  LayoutRegion *lr = &table->regions[0];
  *lr++ = isl->header;
  *lr++ = isl->indexPageMap;
  unsigned int z;
  for (z = 0; z < isl->numZones; ++z) {
    *lr++ = isl->masterIndexZones[z];
  }
  if (isl->openChapter) {
    *lr++ = *isl->openChapter;
  }
  if (isl->freeSpace.numBlocks > 0) {
    *lr++ = isl->freeSpace;
  }

  result = ASSERT((lr == &table->regions[numRegions]),
                  "incorrect number of ISL regions");
  if (result != UDS_SUCCESS) {
    return result;
  }

  *numRegionsPtr = numRegions;
  *tablePtr = table;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static unsigned int regionTypeForSaveType(IndexSaveType saveType)
{
  switch (saveType) {
    case IS_SAVE:
      return RH_TYPE_SAVE;

    case IS_CHECKPOINT:
      return RH_TYPE_CHECKPOINT;

    default:
      break;
  }

  return RH_TYPE_UNSAVED;
}

/*****************************************************************************/
__attribute__((warn_unused_result))
static int writeIndexSaveHeader(IndexSaveLayout *isl,
                                RegionTable     *table,
                                unsigned int     numRegions,
                                BufferedWriter  *writer)
{
  size_t payload = sizeof(isl->saveData);
  if (isl->indexStateBuffer != NULL) {
    payload += contentLength(isl->indexStateBuffer);
  }

  table->header = (RegionHeader) {
    .magic        = REGION_MAGIC,
    .regionBlocks = isl->indexSave.numBlocks,
    .type         = regionTypeForSaveType(isl->saveType),
    .version      = 1,
    .numRegions   = numRegions,
    .payload      = payload,
  };

  size_t tableSize = sizeof(RegionTable) + numRegions * sizeof(LayoutRegion);
  Buffer *buffer;
  int result = makeBuffer(tableSize, &buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = encodeRegionHeader(buffer, &table->header);
  if (result != UDS_SUCCESS) {
    freeBuffer(&buffer);
    return result;
  }

  unsigned int i;
  for (i = 0; i < numRegions; i++) {
    result = encodeLayoutRegion(buffer, &table->regions[i]);
    if (result != UDS_SUCCESS) {
      freeBuffer(&buffer);
      return result;
    }
  }
  result = ASSERT_LOG_ONLY(contentLength(buffer) == tableSize,
                           "%zu bytes encoded of %zu expected",
                           contentLength(buffer), tableSize);
  if (result != UDS_SUCCESS) {
    freeBuffer(&buffer);
    return result;
  }

  result = writeToBufferedWriter(writer,  getBufferContents(buffer),
                                 contentLength(buffer));
  freeBuffer(&buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = makeBuffer(sizeof(isl->saveData), &buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = encodeIndexSaveData(buffer,  &isl->saveData);
  if (result != UDS_SUCCESS) {
    freeBuffer(&buffer);
    return result;
  }

  result = writeToBufferedWriter(writer, getBufferContents(buffer),
                                 contentLength(buffer));
  freeBuffer(&buffer);
  if (result != UDS_SUCCESS) {
    return result;
  }

  if (isl->indexStateBuffer != NULL) {
    result = writeToBufferedWriter(writer,
                                   getBufferContents(isl->indexStateBuffer),
                                   contentLength(isl->indexStateBuffer));
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  return flushBufferedWriter(writer);
}

/*****************************************************************************/
static int writeIndexSaveLayout(IndexLayout *layout, IndexSaveLayout *isl)
{
  unsigned int  numRegions;
  RegionTable  *table;
  int result = makeIndexSaveRegionTable(isl, &numRegions, &table);
  if (result != UDS_SUCCESS) {
    return result;
  }

  BufferedWriter *writer = NULL;
  result = openLayoutWriter(layout, &isl->header, &writer);
  if (result != UDS_SUCCESS) {
    FREE(table);
    return result;
  }

  result = writeIndexSaveHeader(isl, table, numRegions, writer);
  FREE(table);
  freeBufferedWriter(writer);

  isl->written = true;
  return result;
}

/*****************************************************************************/
int commitIndexSave(IndexLayout *layout, unsigned int saveSlot)
{
  int result = ASSERT((saveSlot < layout->super.maxSaves),
                      "save slot out of range");
  if (result != UDS_SUCCESS) {
    return result;
  }

  IndexSaveLayout *isl = &layout->index.saves[saveSlot];

  if (bufferUsed(isl->indexStateBuffer) == 0) {
    return logErrorWithStringError(UDS_UNEXPECTED_RESULT,
                                   "%s: no index state data saved", __func__);
  }

  return writeIndexSaveLayout(layout, isl);
}

/*****************************************************************************/

static void mutilateIndexSaveInfo(IndexSaveLayout *isl)
{
  memset(&isl->saveData, 0, sizeof(isl->saveData));
  isl->read = isl->written = 0;
  isl->saveType = NO_SAVE;
  isl->numZones = 0;
  freeBuffer(&isl->indexStateBuffer);
}

/*****************************************************************************/
int cancelIndexSave(IndexLayout *layout, unsigned int saveSlot)
{
  int result = ASSERT((saveSlot < layout->super.maxSaves),
                      "save slot out of range");
  if (result != UDS_SUCCESS) {
    return result;
  }

  mutilateIndexSaveInfo(&layout->index.saves[saveSlot]);

  return UDS_SUCCESS;
}

/*****************************************************************************/
int discardIndexSaves(IndexLayout *layout, bool all)
{
  int result = UDS_SUCCESS;
  SubIndexLayout *sil = &layout->index;

  if (all) {
    unsigned int i;
    for (i = 0; i < layout->super.maxSaves; ++i) {
      IndexSaveLayout *isl = &sil->saves[i];
      result = firstError(result, invalidateOldSave(layout, isl));
    }
  } else {
    IndexSaveLayout *isl;
    result = selectLatestIndexSaveLayout(sil, layout->super.maxSaves, &isl);
    if (result == UDS_SUCCESS) {
      result = invalidateOldSave(layout, isl);
    }
  }

  return result;
}

/*****************************************************************************/
static int createIndexLayout(IndexLayout            *layout,
                             uint64_t                size,
                             const UdsConfiguration  config)
{
  if (config == NULL) {
    return UDS_CONF_PTR_REQUIRED;
  }

  SaveLayoutSizes sizes;
  int result = computeSizes(&sizes, config, UDS_BLOCK_SIZE, 0);
  if (result != UDS_SUCCESS) {
    return result;
  }

  if (size < sizes.totalBlocks * sizes.blockSize) {
    return logErrorWithStringError(UDS_INSUFFICIENT_INDEX_SPACE,
                                   "layout requires at least %" PRIu64 
                                   " bytes",
                                   sizes.totalBlocks * sizes.blockSize);
  }

  result = initSingleFileLayout(layout, layout->offset, size, &sizes);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = saveSingleFileConfiguration(layout);
  if (result != UDS_SUCCESS) {
    return result;
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
Buffer *getIndexStateBuffer(IndexLayout *layout, unsigned int slot)
{
  return layout->index.saves[slot].indexStateBuffer;
}

/*****************************************************************************/
static int findLayoutRegion(IndexLayout   *layout,
                            unsigned int   slot,
                            const char    *operation,
                            RegionKind     kind,
                            unsigned int   zone,
                            LayoutRegion **lrPtr)
{
  int result = ASSERT((slot < layout->super.maxSaves), "%s not started",
                  operation);
  if (result != UDS_SUCCESS) {
    return result;
  }

  IndexSaveLayout *isl = &layout->index.saves[slot];

  LayoutRegion *lr = NULL;
  switch (kind) {
    case RL_KIND_INDEX_PAGE_MAP:
      lr = &isl->indexPageMap;
      break;

    case RL_KIND_OPEN_CHAPTER:
      if (isl->openChapter == NULL) {
        return logErrorWithStringError(UDS_UNEXPECTED_RESULT,
                                       "%s: %s has no open chapter",
                                       __func__, operation);
      }
      lr = isl->openChapter;
      break;

    case RL_KIND_MASTER_INDEX:
      if (isl->masterIndexZones == NULL || zone >= isl->numZones) {
        return logErrorWithStringError(UDS_UNEXPECTED_RESULT,
                                       "%s: %s has no master index zone %u",
                                       __func__, operation, zone);
      }
      lr = &isl->masterIndexZones[zone];
      break;

    default:
      return logErrorWithStringError(UDS_INVALID_ARGUMENT,
                                     "%s: unexpected kind %u",
                                     __func__, kind);
  }

  *lrPtr = lr;
  return UDS_SUCCESS;
}

/*****************************************************************************/
int openIndexBufferedReader(IndexLayout     *layout,
                            unsigned int     slot,
                            RegionKind       kind,
                            unsigned int     zone,
                            BufferedReader **readerPtr)
{
  LayoutRegion *lr = NULL;
  int result = findLayoutRegion(layout, slot, "load", kind, zone, &lr);
  if (result != UDS_SUCCESS) {
    return result;
  }
  return openLayoutReader(layout, lr, readerPtr);
}

/*****************************************************************************/
int openIndexBufferedWriter(IndexLayout     *layout,
                            unsigned int     slot,
                            RegionKind       kind,
                            unsigned int     zone,
                            BufferedWriter **writerPtr)
{
  LayoutRegion *lr = NULL;
  int result = findLayoutRegion(layout, slot, "save", kind, zone, &lr);
  if (result != UDS_SUCCESS) {
    return result;
  }
  return openLayoutWriter(layout, lr, writerPtr);
}

/*****************************************************************************/
int makeIndexLayoutFromFactory(IOFactory               *factory,
                               off_t                    offset,
                               uint64_t                 namedSize,
                               bool                     newLayout,
                               const UdsConfiguration   config,
                               IndexLayout            **layoutPtr)
{
  // Get the device size and round it down to a multiple of UDS_BLOCK_SIZE.
  size_t size = getWritableSize(factory) & -UDS_BLOCK_SIZE;
  if (namedSize > size) {
    return logErrorWithStringError(UDS_INSUFFICIENT_INDEX_SPACE,
                                   "index storage (%zu) is smaller than the"
                                   " requested size %" PRIu64,
                                   size, namedSize);
  }
  if ((namedSize > 0) && (namedSize < size)) {
    size = namedSize;
  }

  // Get the index size according the the config
  uint64_t configSize;
  int result = udsComputeIndexSize(config, 0, &configSize);
  if (result != UDS_SUCCESS) {
    return result;
  }
  if (size < configSize) {
    return logErrorWithStringError(UDS_INSUFFICIENT_INDEX_SPACE,
                                   "index storage (%zu) is smaller than the"
                                   " required size %" PRIu64,
                                   size, configSize);
  }
  size = configSize;

  IndexLayout *layout = NULL;
  result = ALLOCATE(1, IndexLayout, __func__, &layout);
  if (result != UDS_SUCCESS) {
    return result;
  }
  layout->refCount = 1;

  getIOFactory(factory);
  layout->factory = factory;
  layout->offset  = offset;

  if (newLayout) {
    // Populate the layout from the UDSConfiguration
    result = createIndexLayout(layout, size, config);
  } else {
    // Populate the layout from the saved index.
    result = loadIndexLayout(layout);
  }
  if (result != UDS_SUCCESS) {
    putIndexLayout(&layout);
    return result;
  }
  *layoutPtr = layout;
  return UDS_SUCCESS;
}
