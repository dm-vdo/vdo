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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoConfig.c#4 $
 */

#include <uuid/uuid.h>

#include "vdoConfig.h"

#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"
#include "timeUtils.h"

#include "blockMap.h"
#include "blockMapInternals.h"
#include "constants.h"
#include "forest.h"
#include "numUtils.h"
#include "recoveryJournal.h"
#include "releaseVersions.h"
#include "slab.h"
#include "slabDepot.h"
#include "slabSummary.h"
#include "statusCodes.h"
#include "vdoInternal.h"
#include "vdoLayout.h"
#include "volumeGeometry.h"

/**********************************************************************/
int makeVDOLayoutFromConfig(const VDOConfig      *config,
                            PhysicalBlockNumber   startingOffset,
                            VDOLayout           **vdoLayoutPtr)
{
  VDOLayout *vdoLayout;
  int result = makeVDOLayout(config->physicalBlocks, startingOffset,
                             DEFAULT_BLOCK_MAP_TREE_ROOT_COUNT,
                             config->recoveryJournalSize,
                             getSlabSummarySize(VDO_BLOCK_SIZE), &vdoLayout);
  if (result != VDO_SUCCESS) {
    return result;
  }

  *vdoLayoutPtr = vdoLayout;
  return VDO_SUCCESS;
}

/**
 * Configure a new VDO.
 *
 * @param vdo  The VDO to configure
 *
 * @return VDO_SUCCESS or an error
 **/
__attribute__((warn_unused_result))
static int configureVDO(VDO *vdo)
{
  // The layout starts 1 block past the beginning of the data region, as the
  // data region contains the super block but the layout does not.
  int result = makeVDOLayoutFromConfig(&vdo->config,
                                       getFirstBlockOffset(vdo) + 1,
                                       &vdo->layout);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = makeRecoveryJournal(vdo->nonce, vdo->layer,
                               getVDOPartition(vdo->layout,
                                               RECOVERY_JOURNAL_PARTITION),
                               vdo->completeRecoveries,
                               vdo->config.recoveryJournalSize,
                               RECOVERY_JOURNAL_TAIL_BUFFER_SIZE,
                               &vdo->readOnlyContext, getThreadConfig(vdo),
                               &vdo->recoveryJournal);
  if (result != VDO_SUCCESS) {
    return result;
  }

  SlabConfig slabConfig;
  result = configureSlab(vdo->config.slabSize, vdo->config.slabJournalBlocks,
                         &slabConfig);
  if (result != VDO_SUCCESS) {
    return result;
  }

  Partition *depotPartition = getVDOPartition(vdo->layout,
                                              BLOCK_ALLOCATOR_PARTITION);
  BlockCount depotSize = getFixedLayoutPartitionSize(depotPartition);
  PhysicalBlockNumber origin = getFixedLayoutPartitionOffset(depotPartition);
  result = makeSlabDepot(depotSize, origin, slabConfig, getThreadConfig(vdo),
                         vdo->nonce, 1, vdo->layer, NULL,
                         &vdo->readOnlyContext, vdo->recoveryJournal,
                         &vdo->depot);
  if (result != VDO_SUCCESS) {
    return result;
  }

  if (vdo->config.logicalBlocks == 0) {
    BlockCount dataBlocks
      = slabConfig.dataBlocks * calculateSlabCount(vdo->depot);
    vdo->config.logicalBlocks
      = dataBlocks - computeForestSize(dataBlocks,
                                       DEFAULT_BLOCK_MAP_TREE_ROOT_COUNT);
  }

  Partition *blockMapPartition = getVDOPartition(vdo->layout,
                                                 BLOCK_MAP_PARTITION);
  result = makeBlockMap(vdo->config.logicalBlocks, getThreadConfig(vdo), 0,
                        getFixedLayoutPartitionOffset(blockMapPartition),
                        getFixedLayoutPartitionSize(blockMapPartition),
                        &vdo->blockMap);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = makeSuperBlock(vdo->layer, &vdo->superBlock);
  if (result != VDO_SUCCESS) {
    return result;
  }

  vdo->state = VDO_NEW;
  return VDO_SUCCESS;
}

/**********************************************************************/
int formatVDO(const VDOConfig *config,
              IndexConfig     *indexConfig,
              PhysicalLayer   *layer,
              BlockCount      *logicalBlocksPtr)
{
  STATIC_ASSERT(sizeof(uuid_t) == sizeof(UUID));

  // Generate a UUID.
  uuid_t uuid;
  uuid_generate(uuid);

  return formatVDOWithNonce(config, indexConfig, layer, nowUsec(), uuid,
                            logicalBlocksPtr);
}

/**
 * Clear a partition by writing zeros to every block in that partition.
 *
 * @param layer   The underlying layer
 * @param layout  The VDOLayout
 * @param id      The ID of the partition to clear
 *
 * @return VDO_SUCCESS or an error code
 **/
__attribute__((warn_unused_result))
static int clearPartition(PhysicalLayer *layer,
                          VDOLayout     *layout,
                          PartitionID    id)
{
  Partition           *partition = getVDOPartition(layout, id);
  BlockCount           size      = getFixedLayoutPartitionSize(partition);
  PhysicalBlockNumber  start     = getFixedLayoutPartitionOffset(partition);

  BlockCount bufferBlocks = 1;
  for (BlockCount n = size;
       (bufferBlocks < 4096) && ((n & 0x1) == 0);
       n >>= 1) {
    bufferBlocks <<= 1;
  }

  char *zeroBuffer;
  int result = layer->allocateIOBuffer(layer, bufferBlocks * VDO_BLOCK_SIZE,
                                       "zero buffer", &zeroBuffer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  for (PhysicalBlockNumber pbn = start;
       (pbn < start + size) && (result == VDO_SUCCESS);
       pbn += bufferBlocks) {
    result = layer->writer(layer, pbn, bufferBlocks, zeroBuffer, NULL);
  }

  FREE(zeroBuffer);
  return result;
}

/**
 * Construct a VDO and write out its super block.
 *
 * @param [in]  config            The configuration parameters for the VDO
 * @param [in]  layer             The physical layer the VDO will sit on
 * @param [in]  geometry          The geometry of the physical layer
 * @param [out] logicalBlocksPtr  If not NULL, will be set to the number of
 *                                logical blocks the VDO was formatted to have
 **/
static int makeAndWriteVDO(const VDOConfig      *config,
                           PhysicalLayer        *layer,
                           VolumeGeometry       *geometry,
                           BlockCount           *logicalBlocksPtr)
{
  VDO *vdo;
  int result = makeVDO(layer, &vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  vdo->config                      = *config;
  vdo->nonce                       = geometry->nonce;
  vdo->loadConfig.firstBlockOffset = getDataRegionOffset(*geometry);
  result = configureVDO(vdo);
  if (result != VDO_SUCCESS) {
    freeVDO(&vdo);
    return result;
  }

  result = clearPartition(layer, vdo->layout, BLOCK_MAP_PARTITION);
  if (result != VDO_SUCCESS) {
    logErrorWithStringError(result, "cannot clear block map partition");
    freeVDO(&vdo);
    return result;
  }

  result = clearPartition(layer, vdo->layout, RECOVERY_JOURNAL_PARTITION);
  if (result != VDO_SUCCESS) {
    logErrorWithStringError(result, "cannot clear recovery journal partition");
    freeVDO(&vdo);
    return result;
  }

  result = saveVDOComponents(vdo);
  if (result != VDO_SUCCESS) {
    freeVDO(&vdo);
    return result;
  }

  if (logicalBlocksPtr != NULL) {
    *logicalBlocksPtr = vdo->config.logicalBlocks;
  }

  freeVDO(&vdo);
  return VDO_SUCCESS;
}

/**********************************************************************/
int formatVDOWithNonce(const VDOConfig *config,
                       IndexConfig     *indexConfig,
                       PhysicalLayer   *layer,
                       Nonce            nonce,
                       UUID             uuid,
                       BlockCount      *logicalBlocksPtr)
{
  int result = registerStatusCodes();
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = validateVDOConfig(config, layer->getBlockCount(layer), false);
  if (result != VDO_SUCCESS) {
    return result;
  }

  VolumeGeometry geometry;
  result = initializeVolumeGeometry(nonce, uuid, indexConfig, &geometry);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = clearVolumeGeometry(layer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = makeAndWriteVDO(config, layer, &geometry, logicalBlocksPtr);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = writeVolumeGeometry(layer, &geometry);
  return result;
}

/**
 * Load the super block and decode the VDO component.
 *
 * @param vdo  The vdo containing the super block
 *
 * @return VDO_SUCCESS or an error if the super block could not be read
 **/
__attribute__((warn_unused_result))
static int prepareSuperBlock(VDO *vdo)
{
  VolumeGeometry geometry;
  int result = loadVolumeGeometry(vdo->layer, &geometry);
  if (result != VDO_SUCCESS) {
    return result;
  }

  setLoadConfigFromGeometry(&geometry, &vdo->loadConfig);
  result = loadSuperBlock(vdo->layer, getFirstBlockOffset(vdo),
                          &vdo->superBlock);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = validateVDOVersion(vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  return decodeVDOComponent(vdo);
}

/**
 * Change the state of an inactive VDO image.
 *
 * @param layer            A physical layer
 * @param requireReadOnly  Whether the existing VDO must be in read-only mode
 * @param newState         The new state to store in the VDO
 **/
__attribute__((warn_unused_result))
static int updateVDOSuperBlockState(PhysicalLayer *layer,
                                    bool           requireReadOnly,
                                    VDOState       newState)
{
  VDO *vdo;
  int result = makeVDO(layer, &vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = prepareSuperBlock(vdo);
  if (result != VDO_SUCCESS) {
    freeVDO(&vdo);
    return result;
  }

  if (requireReadOnly && !inReadOnlyMode(vdo)) {
    freeVDO(&vdo);
    return logErrorWithStringError(VDO_NOT_READ_ONLY,
                                   "Can't force rebuild on a normal VDO");
  }

  vdo->state = newState;

  result = saveReconfiguredVDO(vdo);
  freeVDO(&vdo);
  return result;
}

/**********************************************************************/
int forceVDORebuild(PhysicalLayer *layer)
{
  return updateVDOSuperBlockState(layer, true, VDO_FORCE_REBUILD);
}

/**********************************************************************/
int setVDOReadOnlyMode(PhysicalLayer *layer)
{
  return updateVDOSuperBlockState(layer, false, VDO_READ_ONLY_MODE);
}
