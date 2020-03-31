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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoConfig.c#22 $
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
int makeVDOLayoutFromConfig(const struct vdo_config  *config,
                            PhysicalBlockNumber       startingOffset,
                            struct vdo_layout       **vdoLayoutPtr)
{
  struct vdo_layout *vdoLayout;
  int result = make_vdo_layout(config->physical_blocks, startingOffset,
                               DEFAULT_BLOCK_MAP_TREE_ROOT_COUNT,
                               config->recovery_journal_size,
                               get_slab_summary_size(VDO_BLOCK_SIZE),
                               &vdoLayout);
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
static int configureVDO(struct vdo *vdo)
{
  // The layout starts 1 block past the beginning of the data region, as the
  // data region contains the super block but the layout does not.
  int result = makeVDOLayoutFromConfig(&vdo->config,
                                       get_first_block_offset(vdo) + 1,
                                       &vdo->layout);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = make_recovery_journal(vdo->nonce, vdo->layer,
                                 get_vdo_partition(vdo->layout,
                                                   RECOVERY_JOURNAL_PARTITION),
                                 vdo->complete_recoveries,
                                 vdo->config.recovery_journal_size,
                                 RECOVERY_JOURNAL_TAIL_BUFFER_SIZE,
                                 vdo->read_only_notifier, get_thread_config(vdo),
                                 &vdo->recovery_journal);
  if (result != VDO_SUCCESS) {
    return result;
  }

  struct slab_config slabConfig;
  result = configure_slab(vdo->config.slab_size,
                          vdo->config.slab_journal_blocks,
                          &slabConfig);
  if (result != VDO_SUCCESS) {
    return result;
  }

  struct partition *depotPartition
    = get_vdo_partition(vdo->layout, BLOCK_ALLOCATOR_PARTITION);
  BlockCount depotSize = get_fixed_layout_partition_size(depotPartition);
  PhysicalBlockNumber origin = get_fixed_layout_partition_offset(depotPartition);
  result = make_slab_depot(depotSize, origin, slabConfig, get_thread_config(vdo),
                           vdo->nonce, 1, vdo->layer, NULL,
                           vdo->read_only_notifier, vdo->recovery_journal,
                           &vdo->state, &vdo->depot);
  if (result != VDO_SUCCESS) {
    return result;
  }

  if (vdo->config.logical_blocks == 0) {
    BlockCount dataBlocks
      = slabConfig.data_blocks * calculate_slab_count(vdo->depot);
    vdo->config.logical_blocks
      = dataBlocks - compute_forest_size(dataBlocks,
                                         DEFAULT_BLOCK_MAP_TREE_ROOT_COUNT);
  }

  struct partition *blockMapPartition
    = get_vdo_partition(vdo->layout, BLOCK_MAP_PARTITION);
  result = make_block_map(vdo->config.logical_blocks, get_thread_config(vdo), 0,
                          get_fixed_layout_partition_offset(blockMapPartition),
                          get_fixed_layout_partition_size(blockMapPartition),
                          &vdo->block_map);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = make_super_block(vdo->layer, &vdo->super_block);
  if (result != VDO_SUCCESS) {
    return result;
  }

  set_vdo_state(vdo, VDO_NEW);
  return VDO_SUCCESS;
}

/**********************************************************************/
int formatVDO(const struct vdo_config *config,
              struct index_config     *indexConfig,
              PhysicalLayer           *layer)
{
  STATIC_ASSERT(sizeof(uuid_t) == sizeof(UUID));

  // Generate a UUID.
  uuid_t uuid;
  uuid_generate(uuid);

  return formatVDOWithNonce(config, indexConfig, layer, nowUsec(), uuid);
}

/**
 * Clear a partition by writing zeros to every block in that partition.
 *
 * @param layer   The underlying layer
 * @param layout  The vdo_layout
 * @param id      The ID of the partition to clear
 *
 * @return VDO_SUCCESS or an error code
 **/
__attribute__((warn_unused_result))
static int clearPartition(PhysicalLayer     *layer,
                          struct vdo_layout *layout,
                          PartitionID        id)
{
  struct partition    *partition = get_vdo_partition(layout, id);
  BlockCount           size      = get_fixed_layout_partition_size(partition);
  PhysicalBlockNumber  start     = get_fixed_layout_partition_offset(partition);

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
 * @param config            The configuration parameters for the VDO
 * @param layer             The physical layer the VDO will sit on
 * @param geometry          The geometry of the physical layer
 **/
static int makeAndWriteVDO(const struct vdo_config *config,
                           PhysicalLayer           *layer,
                           struct volume_geometry  *geometry)
{
  struct vdo *vdo;
  int result = make_vdo(layer, &vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  vdo->config                      = *config;
  vdo->nonce                       = geometry->nonce;
  vdo->load_config.first_block_offset = get_data_region_offset(*geometry);
  result = configureVDO(vdo);
  if (result != VDO_SUCCESS) {
    free_vdo(&vdo);
    return result;
  }

  result = clearPartition(layer, vdo->layout, BLOCK_MAP_PARTITION);
  if (result != VDO_SUCCESS) {
    logErrorWithStringError(result, "cannot clear block map partition");
    free_vdo(&vdo);
    return result;
  }

  result = clearPartition(layer, vdo->layout, RECOVERY_JOURNAL_PARTITION);
  if (result != VDO_SUCCESS) {
    logErrorWithStringError(result, "cannot clear recovery journal partition");
    free_vdo(&vdo);
    return result;
  }

  result = save_vdo_components(vdo);
  if (result != VDO_SUCCESS) {
    free_vdo(&vdo);
    return result;
  }

  free_vdo(&vdo);
  return VDO_SUCCESS;
}

/**********************************************************************/
int formatVDOWithNonce(const struct vdo_config *config,
                       struct index_config     *indexConfig,
                       PhysicalLayer           *layer,
                       Nonce                    nonce,
                       UUID                     uuid)
{
  int result = register_status_codes();
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = validate_vdo_config(config, layer->getBlockCount(layer), false);
  if (result != VDO_SUCCESS) {
    return result;
  }

  struct volume_geometry geometry;
  result = initialize_volume_geometry(nonce, uuid, indexConfig, &geometry);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = clear_volume_geometry(layer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = makeAndWriteVDO(config, layer, &geometry);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = write_volume_geometry(layer, &geometry);
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
static int prepareSuperBlock(struct vdo *vdo)
{
  struct volume_geometry geometry;
  int result = load_volume_geometry(vdo->layer, &geometry);
  if (result != VDO_SUCCESS) {
    return result;
  }

  setLoadConfigFromGeometry(&geometry, &vdo->load_config);
  result = load_super_block(vdo->layer, get_first_block_offset(vdo),
                            &vdo->super_block);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = validate_vdo_version(vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  return decode_vdo_component(vdo);
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
  struct vdo *vdo;
  int result = make_vdo(layer, &vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = prepareSuperBlock(vdo);
  if (result != VDO_SUCCESS) {
    free_vdo(&vdo);
    return result;
  }

  if (requireReadOnly && !in_read_only_mode(vdo)) {
    free_vdo(&vdo);
    return logErrorWithStringError(VDO_NOT_READ_ONLY,
                                   "Can't force rebuild on a normal VDO");
  }

  set_vdo_state(vdo, newState);

  result = save_reconfigured_vdo(vdo);
  free_vdo(&vdo);
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
