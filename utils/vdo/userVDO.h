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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/userVDO.h#4 $
 */

#ifndef USER_VDO_H
#define USER_VDO_H

#include "slabSummaryFormat.h"
#include "types.h"
#include "vdoComponentStates.h"
#include "vdoInternal.h"

/**
 * A representation of a VDO for use by user space tools.
 **/
typedef struct user_vdo {
  /* The full state of all components */
  struct vdo_component_states  states;
  /* The physical storage below the VDO */
  PhysicalLayer               *layer;

  unsigned int                slabSizeShift;
  slab_count_t                slabCount;
  uint64_t                    slabOffsetMask;
  /* The base vdo structure, will go away once no user tools need it */
  struct vdo                  *vdo;
} UserVDO;

/**
 * Construct a user space VDO object.
 *
 * @param layer   The layer from which to read and write the VDO
 * @param vdoPtr  A pointer to hold the VDO
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check makeUserVDO(PhysicalLayer *layer, UserVDO **vdoPtr);

/**
 * Free a user space VDO object and NULL out the reference to it.
 *
 * @param vdoPtr  A poitner to the VDO to free
 **/
void freeUserVDO(UserVDO **vdoPtr);

/**
 * Set the slab parameters which are derived from the vdo config and the
 * slab config.
 *
 * @param vdo  The vdo
 **/
void setDerivedSlabParameters(UserVDO *vdo);

/**
 * Get the slab number for a pbn.
 *
 * @param vdo       The vdo
 * @param pbn       The pbn in question
 * @param slab_ptr  A pointer to hold the slab number
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check getSlabNumber(const UserVDO           *vdo,
                               physical_block_number_t  pbn,
                               slab_count_t            *slabPtr);


/**
 * Get the slab block number for a pbn.
 *
 * @param vdo      The vdo
 * @param pbn      The pbn in question
 * @param sbn_ptr  A pointer to hold the slab block number
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check getSlabBlockNumber(const UserVDO           *vdo,
                                    physical_block_number_t  pbn,
                                    slab_block_number       *sbnPtr);

/**
 * Check whether a given PBN is a valid PBN for a data block. This
 * recapitulates is_physical_data_block().
 *
 * @param vdo  The vdo
 * @param pbn  The PBN to check
 *
 * @return true if the PBN can be used for a data block
 **/
bool __must_check
isValidDataBlock(const UserVDO *vdo, physical_block_number_t pbn);

/**
 * Get a partition from the VDO or fail with an error.
 *
 * @param vdo  The VDO
 * @param partitionID  The ID of the desired partition
 * @param errorMessage The error message if the partition does not exist
 **/
const struct partition * __must_check
getPartition(const UserVDO *vdo,
             partition_id   id,
             const char    *errorMessage);

#endif /* USER_VDO_H */