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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/blockMapUtils.c#5 $
 */

#include "blockMapUtils.h"

#include <err.h>

#include "syscalls.h"
#include "memoryAlloc.h"

#include "blockMapInternals.h"
#include "blockMapPage.h"
#include "physicalLayer.h"
#include "slab.h"
#include "slabDepotInternals.h"
#include "vdoLayout.h"
#include "vdoInternal.h"

/**********************************************************************/
bool isValidDataBlock(const SlabDepot *depot, PhysicalBlockNumber pbn)
{
  if ((pbn < depot->firstBlock) || (pbn >= depot->lastBlock)) {
    return false;
  }

  PhysicalBlockNumber sbnMask = (1ULL << depot->slabSizeShift) - 1;
  SlabBlockNumber     sbn     = (pbn - depot->firstBlock) & sbnMask;
  return (sbn < getSlabConfig(depot)->dataBlocks);
}

/**
 * Read a block map page call the examiner on every defined mapping in it.
 * Also recursively call itself to examine an entire tree.
 *
 * @param vdo       The VDO
 * @param pagePBN   The PBN of the block map page to read
 * @param height    The height of this page in the tree
 * @param examiner  The MappingExaminer to call for each mapped entry
 *
 * @return VDO_SUCCESS or an error
 **/
static int readAndExaminePage(VDO                 *vdo,
                              PhysicalBlockNumber  pagePBN,
                              Height               height,
                              MappingExaminer     *examiner)
{
  BlockMapPage *page;
  int result = vdo->layer->allocateIOBuffer(vdo->layer, VDO_BLOCK_SIZE,
                                            "block map page", (char **) &page);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = readBlockMapPage(vdo->layer, pagePBN, vdo->nonce, page);
  if (result != VDO_SUCCESS) {
    FREE(page);
    return result;
  }

  if (!isBlockMapPageInitialized(page)) {
    FREE(page);
    return VDO_SUCCESS;
  }

  BlockMapSlot blockMapSlot = {
    .pbn  = pagePBN,
    .slot = 0,
  };
  for (; blockMapSlot.slot < BLOCK_MAP_ENTRIES_PER_PAGE; blockMapSlot.slot++) {
    DataLocation mapped
      = unpackBlockMapEntry(&page->entries[blockMapSlot.slot]);

    result = examiner(blockMapSlot, height, mapped.pbn, mapped.state);
    if (result != VDO_SUCCESS) {
      FREE(page);
      return result;
    }

    if (!isMappedLocation(&mapped)) {
      continue;
    }

    if ((height > 0) && isValidDataBlock(vdo->depot, mapped.pbn)) {
      result = readAndExaminePage(vdo, mapped.pbn, height - 1, examiner);
      if (result != VDO_SUCCESS) {
        FREE(page);
        return result;
      }
    }
  }

  FREE(page);
  return VDO_SUCCESS;
}

/**********************************************************************/
int examineBlockMapEntries(VDO *vdo, MappingExaminer *examiner)
{
  // Examine flat pages.
  BlockMap  *map           = getBlockMap(vdo);
  PageCount  flatPageCount = map->flatPageCount;

  for (PageNumber pageNumber = 0; pageNumber < flatPageCount; pageNumber++) {
    PhysicalBlockNumber pbn = pageNumber + BLOCK_MAP_FLAT_PAGE_ORIGIN;
    int result = readAndExaminePage(vdo, pbn, 0, examiner);
    if (result != VDO_SUCCESS) {
      return result;
    }
  }

  int result = ASSERT((map->rootOrigin != 0),
                      "block map root origin must be non-zero");
  if (result != VDO_SUCCESS) {
    return result;
  }
  result = ASSERT((map->rootCount != 0),
                  "block map root count must be non-zero");
  if (result != VDO_SUCCESS) {
    return result;
  }

  Height height = BLOCK_MAP_TREE_HEIGHT - 1;
  for (uint8_t rootIndex = 0; rootIndex < map->rootCount; rootIndex++) {
    result = readAndExaminePage(vdo, rootIndex + map->rootOrigin, height,
                                examiner);
    if (result != VDO_SUCCESS) {
      return result;
    }
  }

  return VDO_SUCCESS;
}

/**
 * Find and decode a particular slot from a block map page.
 *
 * @param vdo           The VDO
 * @param pbn           The PBN of the block map page to read
 * @param slot          The slot to read from the block map page
 * @param mappedPBNPtr  A pointer to the mapped PBN
 * @param mappedPtr     A pointer to the mapped state
 *
 * @return VDO_SUCCESS or an error
 **/
static int readSlotFromPage(VDO                 *vdo,
                            PhysicalBlockNumber  pbn,
                            SlotNumber           slot,
                            PhysicalBlockNumber *mappedPBNPtr,
                            BlockMappingState   *mappedStatePtr)
{
  BlockMapPage *page;
  int result = vdo->layer->allocateIOBuffer(vdo->layer, VDO_BLOCK_SIZE,
                                            "page buffer", (char **) &page);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = readBlockMapPage(vdo->layer, pbn, vdo->nonce, page);
  if (result != VDO_SUCCESS) {
    FREE(page);
    return result;
  }

  DataLocation mapped;
  if (isBlockMapPageInitialized(page)) {
    mapped = unpackBlockMapEntry(&page->entries[slot]);
  } else {
    mapped = (DataLocation) {
      .state = MAPPING_STATE_UNMAPPED,
      .pbn   = ZERO_BLOCK,
    };
  }

  *mappedStatePtr = mapped.state;
  *mappedPBNPtr   = mapped.pbn;

  FREE(page);
  return VDO_SUCCESS;
}

/**********************************************************************/
int findLBNPage(VDO *vdo, LogicalBlockNumber lbn, PhysicalBlockNumber *pbnPtr)
{
  if (lbn >= vdo->config.logicalBlocks) {
    warnx("VDO has only %" PRIu64 " logical blocks, cannot dump mapping for"
          " LBA %" PRIu64, vdo->config.logicalBlocks, lbn);
    return VDO_OUT_OF_RANGE;
  }

  BlockMap *map = getBlockMap(vdo);
  PageNumber pageNumber = lbn / BLOCK_MAP_ENTRIES_PER_PAGE;
  if (pageNumber < map->flatPageCount) {
    // It's in the flat section of the block map.
    *pbnPtr = BLOCK_MAP_FLAT_PAGE_ORIGIN + pageNumber;
    return VDO_SUCCESS;
  }

  // It's in the tree section of the block map.
  SlotNumber slots[BLOCK_MAP_TREE_HEIGHT];
  RootCount rootIndex = pageNumber % map->rootCount;
  pageNumber -= map->flatPageCount;
  for (int i = 1; i < BLOCK_MAP_TREE_HEIGHT; i++) {
    slots[i] = pageNumber % BLOCK_MAP_ENTRIES_PER_PAGE;
    pageNumber /= BLOCK_MAP_ENTRIES_PER_PAGE;
  }

  PhysicalBlockNumber pbn = map->rootOrigin + rootIndex;
  for (int i = BLOCK_MAP_TREE_HEIGHT - 1; i > 0; i--) {
    BlockMappingState state;
    int result = readSlotFromPage(vdo, pbn, slots[i], &pbn, &state);
    if ((result != VDO_SUCCESS) || (pbn == ZERO_BLOCK)
        || (state == MAPPING_STATE_UNMAPPED)) {
      *pbnPtr = ZERO_BLOCK;
      return result;
    }
  }

  *pbnPtr = pbn;
  return VDO_SUCCESS;
}

/**********************************************************************/
int findLBNMapping(VDO                 *vdo,
                   LogicalBlockNumber   lbn,
                   PhysicalBlockNumber *pbnPtr,
                   BlockMappingState   *statePtr)
{
  PhysicalBlockNumber pagePBN;
  int result = findLBNPage(vdo, lbn, &pagePBN);
  if (result != VDO_SUCCESS) {
    return result;
  }

  if (pagePBN == ZERO_BLOCK) {
    *pbnPtr   = ZERO_BLOCK;
    *statePtr = MAPPING_STATE_UNMAPPED;
    return VDO_SUCCESS;
  }

  SlotNumber slot = lbn % BLOCK_MAP_ENTRIES_PER_PAGE;
  return readSlotFromPage(vdo, pagePBN, slot, pbnPtr, statePtr);
}

/**********************************************************************/
int readBlockMapPage(PhysicalLayer        *layer,
                     PhysicalBlockNumber   pbn,
                     Nonce                 nonce,
                     BlockMapPage         *page)
{
  int result = layer->reader(layer, pbn, 1, (char *) page, NULL);
  if (result != VDO_SUCCESS) {
    char errBuf[ERRBUF_SIZE];
    printf("%" PRIu64 " unreadable : %s",
           pbn, stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  BlockMapPageValidity validity = validateBlockMapPage(page, nonce, pbn);
  if (validity == BLOCK_MAP_PAGE_VALID) {
    return VDO_SUCCESS;
  }

  if (validity == BLOCK_MAP_PAGE_BAD) {
    warnx("Expected page %" PRIu64 " but got page %" PRIu64,
          pbn, getBlockMapPagePBN(page));
  }

  markBlockMapPageInitialized(page, false);
  return VDO_SUCCESS;
}
