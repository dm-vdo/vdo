/*
 * Copyright (c) 2017 Red Hat, Inc.
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
 * $Id: //eng/vdo-releases/magnesium/src/c++/vdo/user/vdoConfig.c#2 $
 */

#include "vdoConfig.h"

#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"
#include "timeUtils.h"

#include "blockMap.h"
#include "blockMapInternals.h"
#include "constants.h"
#include "numUtils.h"
#include "recoveryJournal.h"
#include "releaseVersions.h"
#include "slab.h"
#include "slabDepotInternals.h"
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
  PhysicalLayer *layer = vdo->layer;
  // The layout starts 1 block past the beginning of the data region, as the
  // data region contains the super block but the layout does not.
  int result = makeVDOLayoutFromConfig(&vdo->config,
                                       layer->getDataRegionOffset(layer) + 1,
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
                         vdo->nonce, 1, 1, vdo->layer, NULL,
                         &vdo->readOnlyContext, vdo->recoveryJournal,
                         &vdo->depot);
  if (result != VDO_SUCCESS) {
    return result;
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
  return saveVDOComponents(vdo);
}

/**********************************************************************/
int formatVDO(const VDOConfig *config,
              IndexConfig     *indexConfig,
              PhysicalLayer   *layer)
{
  return formatVDOWithNonce(config, indexConfig, layer, nowUsec());
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

/**********************************************************************/
int formatVDOWithNonce(const VDOConfig *config,
                       IndexConfig     *indexConfig,
                       PhysicalLayer   *layer,
                       Nonce            nonce)
{
  int result = registerStatusCodes();
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = validateVDOConfig(config, layer->getBlockCount(layer));
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = writeVolumeGeometry(layer, nonce, indexConfig);
  if (result != VDO_SUCCESS) {
    return result;
  }

  VDO *vdo;
  result = makeVDO(layer, &vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  vdo->config = *config;
  vdo->nonce  = nonce;
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
  }

  freeVDO(&vdo);
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
  int result = loadSuperBlock(vdo->layer, &vdo->superBlock);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = validateVDOVersion(vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  if (upgradeRequired(vdo)) {
    return logErrorWithStringError(VDO_UNSUPPORTED_VERSION,
                                   "Can't reconfigure, "
                                   "VDO version not current");
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
    return logErrorWithStringError(VDO_NOT_CLEAN,
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
