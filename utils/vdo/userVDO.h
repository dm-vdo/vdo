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
 */

#ifndef USER_VDO_H
#define USER_VDO_H

#include "encodings.h"
#include "types.h"

#include "physicalLayer.h"

/**
 * A representation of a VDO for use by user space tools.
 **/
typedef struct user_vdo {
  /* The physical storage below the VDO */
  PhysicalLayer               *layer;
  /* The geometry of the VDO */
  struct volume_geometry       geometry;
  /* The buffer for the super block */
  char                         superBlockBuffer[VDO_BLOCK_SIZE];
  /* The full state of all components */
  struct vdo_component_states  states;

  unsigned int                 slabSizeShift;
  slab_count_t                 slabCount;
  uint64_t                     slabOffsetMask;
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
 * @param vdoPtr  A pointer to the VDO to free
 **/
void freeUserVDO(UserVDO **vdoPtr);

/**
 * Load the volume geometry from a layer.
 *
 * @param layer     The layer from which to read the geometry
 * @param geometry  The structure to receive the decoded fields
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check loadVolumeGeometry(PhysicalLayer *layer, struct volume_geometry *geometry);

/**
 * Read the super block from the location indicated by the geometry.
 *
 * @param vdo  The VDO whose super block is to be read
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check loadSuperBlock(UserVDO *vdo);

/**
 * Load a vdo from a specified super block location.
 *
 * @param [in]  layer           The physical layer the vdo sits on
 * @param [in]  geometry        A pointer to the geometry for the volume
 * @param [in]  validateConfig  Whether to validate the vdo against the layer
 * @param [out] vdoPtr          A pointer to hold the decoded vdo
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check loadVDOWithGeometry(PhysicalLayer           *layer,
                                     struct volume_geometry  *geometry,
                                     bool                     validateConfig,
                                     UserVDO                **vdoPtr);

/**
 * Load a vdo volume.
 *
 * @param [in]  layer           The physical layer the vdo sits on
 * @param [in]  validateConfig  Whether to validate the vdo against the layer
 * @param [out] vdoPtr          A pointer to hold the decoded vdo
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check
loadVDO(PhysicalLayer *layer, bool validateConfig, UserVDO **vdoPtr);

/**
 * Write a specific version of geometry block for a VDO.
 *
 * @param layer     The layer on which to write
 * @param geometry  The volume_geometry to be written
 * @param version   The version of the geometry to write
 *
 * @return VDO_SUCCESS or an error.
 **/
int __must_check writeVolumeGeometryWithVersion(PhysicalLayer          *layer,
                                                struct volume_geometry *geometry,
                                                u32                     version);

/**
 * Write a geometry block for a VDO.
 *
 * @param layer     The layer on which to write
 * @param geometry  The volume_geometry to be written
 *
 * @return VDO_SUCCESS or an error.
 **/
static inline int __must_check
writeVolumeGeometry(PhysicalLayer *layer, struct volume_geometry *geometry)
{
  return writeVolumeGeometryWithVersion(layer, geometry, VDO_DEFAULT_GEOMETRY_BLOCK_VERSION);
}

/**
 * Encode and write out the super block (assuming the components have already
 * been encoded). This method is broken out for unit testing.
 *
 * @param vdo  The vdo whose super block is to be saved
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check saveSuperBlock(UserVDO *vdo);

/**
 * Encode and save the super block and optionally the geometry block of a VDO.
 *
 * @param vdo           The VDO to save
 * @param saveGeometry  If <code>true</code>, write the geometry after writing
 *                      the super block
 **/
int __must_check saveVDO(UserVDO *vdo, bool saveGeometry);

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
 * recapitulates vdo_is_physical_data_block().
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
 * @param vdo           The VDO
 * @param id            The ID of the desired partition
 * @param errorMessage  The error message if the partition does not exist
 **/
const struct partition * __must_check
getPartition(const UserVDO     *vdo,
             enum partition_id  id,
             const char        *errorMessage);

#endif /* USER_VDO_H */
