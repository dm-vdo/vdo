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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoConfig.c#50 $
 */

#include <uuid/uuid.h>

#include "vdoConfig.h"

#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"
#include "timeUtils.h"

#include "blockMapFormat.h"
#include "constants.h"
#include "fixedLayout.h"
#include "numUtils.h"
#include "physicalLayer.h"
#include "recoveryJournalFormat.h"
#include "releaseVersions.h"
#include "slabDepotFormat.h"
#include "slabSummaryFormat.h"
#include "statusCodes.h"
#include "vdoComponentStates.h"
#include "vdoState.h"
#include "volumeGeometry.h"

#include "userVDO.h"
#include "vdoVolumeUtils.h"

enum {
  RECOVERY_JOURNAL_STARTING_SEQUENCE_NUMBER = 1,
};

/**********************************************************************/
int makeFixedLayoutFromConfig(const struct vdo_config  *config,
                              physical_block_number_t   startingOffset,
                              struct fixed_layout     **layoutPtr)
{
  return make_vdo_fixed_layout(config->physical_blocks, startingOffset,
                               DEFAULT_VDO_BLOCK_MAP_TREE_ROOT_COUNT,
                               config->recovery_journal_size,
                               get_slab_summary_size(VDO_BLOCK_SIZE),
                               layoutPtr);
}

struct recovery_journal_state_7_0 __must_check configureRecoveryJournal(void)
{
  return (struct recovery_journal_state_7_0) {
    .journal_start         = RECOVERY_JOURNAL_STARTING_SEQUENCE_NUMBER,
    .logical_blocks_used   = 0,
    .block_map_data_blocks = 0,
  };
}

/**
 * Compute the approximate number of pages which the forest will allocate in
 * order to map the specified number of logical blocks. This method assumes
 * that the block map is entirely arboreal.
 *
 * @param logicalBlocks  The number of blocks to map
 * @param rootCount      The number of trees in the forest
 *
 * @return A (slight) over-estimate of the total number of possible forest
 *         pages including the leaves
 **/
static block_count_t __must_check
computeForestSize(block_count_t logicalBlocks,
                  root_count_t  rootCount)
{
  struct boundary newSizes;
  block_count_t approximateNonLeaves = compute_new_forest_pages(rootCount,
                                                                NULL,
                                                                logicalBlocks,
                                                                &newSizes);

  // Exclude the tree roots since those aren't allocated from slabs,
  // and also exclude the super-roots, which only exist in memory.
  approximateNonLeaves -=
    rootCount * (newSizes.levels[VDO_BLOCK_MAP_TREE_HEIGHT - 2] +
                 newSizes.levels[VDO_BLOCK_MAP_TREE_HEIGHT - 1]);

  block_count_t approximateLeaves =
    compute_block_map_page_count(logicalBlocks - approximateNonLeaves);

  // This can be a slight over-estimate since the tree will never have to
  // address these blocks, so it might be a tiny bit smaller.
  return (approximateNonLeaves + approximateLeaves);
}

/**
 * Configure a new VDO.
 *
 * @param vdo  The VDO to configure
 *
 * @return VDO_SUCCESS or an error
 **/
static int __must_check configureVDO(UserVDO *vdo)
{
  struct vdo_config *config = &vdo->states.vdo.config;

  // The layout starts 1 block past the beginning of the data region, as the
  // data region contains the super block but the layout does not.
  physical_block_number_t startingOffset =
    get_data_region_offset(vdo->geometry) + 1;
  int result = makeFixedLayoutFromConfig(config, startingOffset,
                                         &vdo->states.layout);
  if (result != VDO_SUCCESS) {
    return result;
  }

  vdo->states.recovery_journal = configureRecoveryJournal();

  struct slab_config slabConfig;
  result = configure_vdo_slab(config->slab_size,
                              config->slab_journal_blocks,
                              &slabConfig);
  if (result != VDO_SUCCESS) {
    return result;
  }

  const struct partition *partition = getPartition(vdo,
                                                   BLOCK_ALLOCATOR_PARTITION,
                                                   "no allocator partition");
  if (result != VDO_SUCCESS) {
    return result;
  }

  result
    = configure_vdo_slab_depot(get_fixed_layout_partition_size(partition),
                               get_fixed_layout_partition_offset(partition),
                               slabConfig, 0, &vdo->states.slab_depot);
  if (result != VDO_SUCCESS) {
    return result;
  }

  setDerivedSlabParameters(vdo);

  if (config->logical_blocks == 0) {
    block_count_t dataBlocks = slabConfig.data_blocks * vdo->slabCount;
    config->logical_blocks
      = dataBlocks - computeForestSize(dataBlocks,
                                       DEFAULT_VDO_BLOCK_MAP_TREE_ROOT_COUNT);
  }

  partition = getPartition(vdo, BLOCK_MAP_PARTITION, "no block map partition");
  vdo->states.block_map = (struct block_map_state_2_0) {
    .flat_page_origin = VDO_BLOCK_MAP_FLAT_PAGE_ORIGIN,
    .flat_page_count = 0,
    .root_origin = get_fixed_layout_partition_offset(partition),
    .root_count = DEFAULT_VDO_BLOCK_MAP_TREE_ROOT_COUNT,
  };

  vdo->states.vdo.state = VDO_NEW;
  return VDO_SUCCESS;
}

/**********************************************************************/
int formatVDO(const struct vdo_config   *config,
              const struct index_config *indexConfig,
              PhysicalLayer             *layer)
{
  // Generate a uuid.
  uuid_t uuid;
  uuid_generate(uuid);

  return formatVDOWithNonce(config, indexConfig, layer, current_time_us(),
                            &uuid);
}

/**********************************************************************/
int calculateMinimumVDOFromConfig(const struct vdo_config   *config,
                                  const struct index_config *indexConfig,
                                  block_count_t             *minVDOBlocks)
{
  // The minimum VDO size is the minimal size of the fixed layout +
  // one slab size for the allocator. The minimum fixed layout size
  // calculated below comes from vdoLayout.c in makeVDOFixedLayout().

  block_count_t indexSize = 0;
  if (indexConfig != NULL) {
    int result = compute_index_blocks(indexConfig, &indexSize);
    if (result != VDO_SUCCESS) {
      return result;
    }
  }

  block_count_t blockMapBlocks = DEFAULT_VDO_BLOCK_MAP_TREE_ROOT_COUNT;
  block_count_t journalBlocks  = config->recovery_journal_size;
  block_count_t summaryBlocks  = get_slab_summary_size(VDO_BLOCK_SIZE);
  block_count_t slabBlocks     = config->slab_size;

  // The +2 takes into account the super block and geometry block.
  block_count_t fixedLayoutSize
    = indexSize + 2 + blockMapBlocks + journalBlocks + summaryBlocks;

  *minVDOBlocks = fixedLayoutSize + slabBlocks;

  return VDO_SUCCESS;
}

/**
 * Clear a partition by writing zeros to every block in that partition.
 *
 * @param vdo  The VDO with the partition to be cleared
 * @param id   The ID of the partition to clear
 *
 * @return VDO_SUCCESS or an error code
 **/
static int __must_check clearPartition(UserVDO *vdo, enum partition_id id)
{
  struct partition *partition;
  int result = get_partition(vdo->states.layout, id, &partition);
  if (result != VDO_SUCCESS) {
    return result;
  }

  block_count_t size = get_fixed_layout_partition_size(partition);
  physical_block_number_t start = get_fixed_layout_partition_offset(partition);

  block_count_t bufferBlocks = 1;
  for (block_count_t n = size;
       (bufferBlocks < 4096) && ((n & 0x1) == 0);
       n >>= 1) {
    bufferBlocks <<= 1;
  }

  char *zeroBuffer;
  result = vdo->layer->allocateIOBuffer(vdo->layer,
                                        bufferBlocks * VDO_BLOCK_SIZE,
                                        "zero buffer", &zeroBuffer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  for (physical_block_number_t pbn = start;
       (pbn < start + size) && (result == VDO_SUCCESS);
       pbn += bufferBlocks) {
    result = vdo->layer->writer(vdo->layer, pbn, bufferBlocks, zeroBuffer);
  }

  FREE(zeroBuffer);
  return result;
}

/**
 * Configure a VDO and its geometry and write it out.
 *
 * @param vdo               The VDO to create
 * @param config            The configuration parameters for the VDO
 * @param indexConfig       The configuration parameters for the index
 * @param nonce             The nonce for the VDO
 * @param uuid              The uuid for the VDO
 **/
static int configureAndWriteVDO(UserVDO                   *vdo,
                                const struct vdo_config   *config,
                                const struct index_config *indexConfig,
                                nonce_t                    nonce,
                                uuid_t                    *uuid)
{
  int result = initialize_volume_geometry(nonce, uuid, indexConfig,
                                          &vdo->geometry);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = clear_volume_geometry(vdo->layer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  vdo->states.vdo.config              = *config;
  vdo->states.vdo.nonce               = nonce;
  vdo->states.volume_version          = VDO_VOLUME_VERSION_67_0;
  result = configureVDO(vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = clearPartition(vdo, BLOCK_MAP_PARTITION);
  if (result != VDO_SUCCESS) {
    return log_error_strerror(result, "cannot clear block map partition");
  }

  result = clearPartition(vdo, RECOVERY_JOURNAL_PARTITION);
  if (result != VDO_SUCCESS) {
    return log_error_strerror(result,
                              "cannot clear recovery journal partition");
  }

  return saveVDO(vdo, true);
}

/**********************************************************************/
int formatVDOWithNonce(const struct vdo_config   *config,
                       const struct index_config *indexConfig,
                       PhysicalLayer             *layer,
                       nonce_t                    nonce,
                       uuid_t                    *uuid)
{
  int result = register_vdo_status_codes();
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = validate_vdo_config(config, layer->getBlockCount(layer), false);
  if (result != VDO_SUCCESS) {
    return result;
  }

  UserVDO *vdo;
  result = makeUserVDO(layer, &vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = configureAndWriteVDO(vdo, config, indexConfig, nonce, uuid);
  freeUserVDO(&vdo);
  return result;
}

/**
 * Change the state of an inactive VDO image.
 *
 * @param layer            A physical layer
 * @param requireReadOnly  Whether the existing VDO must be in read-only mode
 * @param newState         The new state to store in the VDO
 **/
static int __must_check
updateVDOSuperBlockState(PhysicalLayer *layer,
                         bool requireReadOnly,
                         enum vdo_state newState)
{
  UserVDO *vdo;
  int result = loadVDO(layer, false, &vdo);
  if (result != VDO_SUCCESS) {
    return result;
  }

  if (requireReadOnly && (vdo->states.vdo.state != VDO_READ_ONLY_MODE)) {
    freeUserVDO(&vdo);
    return VDO_NOT_READ_ONLY;
  }

  vdo->states.vdo.state = newState;
  result = saveVDO(vdo, false);
  freeUserVDO(&vdo);
  return result;
}

/**********************************************************************/
int forceVDORebuild(PhysicalLayer *layer)
{
  int result = updateVDOSuperBlockState(layer, true, VDO_FORCE_REBUILD);
  if (result == VDO_NOT_READ_ONLY) {
    return log_error_strerror(VDO_NOT_READ_ONLY,
                              "Can't force rebuild on a normal VDO");
  }

  return result;
}

/**********************************************************************/
int setVDOReadOnlyMode(PhysicalLayer *layer)
{
  return updateVDOSuperBlockState(layer, false, VDO_READ_ONLY_MODE);
}
