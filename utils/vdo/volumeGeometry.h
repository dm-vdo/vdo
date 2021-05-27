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
 * $Id: //eng/linux-vdo/src/c++/vdo/base/volumeGeometry.h#26 $
 */

#ifndef VOLUME_GEOMETRY_H
#define VOLUME_GEOMETRY_H


#include <uuid/uuid.h>

#include "uds.h"

#include "types.h"

enum {
	GEOMETRY_BLOCK_LOCATION = 0,
};

struct index_config {
	uint32_t mem;
	uint32_t checkpoint_frequency;
	bool sparse;
} __packed;

enum volume_region_id {
	INDEX_REGION = 0,
	DATA_REGION = 1,
	VOLUME_REGION_COUNT,
};

struct volume_region {
	/** The ID of the region */
	enum volume_region_id id;
	/**
	 * The absolute starting offset on the device. The region continues
	 * until the next region begins.
	 */
	physical_block_number_t start_block;
} __packed;

struct volume_geometry {
	/** The release version number of this volume */
	release_version_number_t release_version;
	/** The nonce of this volume */
	nonce_t nonce;
	/** The uuid of this volume */
	uuid_t uuid;
	/** The block offset to be applied to bios */
	block_count_t bio_offset;
	/** The regions in ID order */
	struct volume_region regions[VOLUME_REGION_COUNT];
	/** The index config */
	struct index_config index_config;
} __packed;

/** This volume geometry struct is used for sizing only */
struct volume_geometry_4_0 {
	/** The release version number of this volume */
	release_version_number_t release_version;
	/** The nonce of this volume */
	nonce_t nonce;
	/** The uuid of this volume */
	uuid_t uuid;
	/** The regions in ID order */
	struct volume_region regions[VOLUME_REGION_COUNT];
	/** The index config */
	struct index_config index_config;
} __packed;

/**
 * Get the start of the index region from a geometry.
 *
 * @param geometry  The geometry
 *
 * @return The start of the index region
 **/
static inline physical_block_number_t __must_check
vdo_get_index_region_start(struct volume_geometry geometry)
{
	return geometry.regions[INDEX_REGION].start_block;
}

/**
 * Get the start of the data region from a geometry.
 *
 * @param geometry  The geometry
 *
 * @return The start of the data region
 **/
static inline physical_block_number_t __must_check
vdo_get_data_region_start(struct volume_geometry geometry)
{
	return geometry.regions[DATA_REGION].start_block;
}

/**
 * Get the size of the index region from a geometry.
 *
 * @param geometry  The geometry
 *
 * @return the size of the index region
 **/
static inline physical_block_number_t __must_check
vdo_get_index_region_size(struct volume_geometry geometry)
{
	return vdo_get_data_region_start(geometry) -
		vdo_get_index_region_start(geometry);
}

/**
 * Decode and validate an encoded geometry block.
 *
 * @param block     The encoded geometry block
 * @param geometry  The structure to receive the decoded fields
 **/
int __must_check
vdo_parse_geometry_block(byte *block, struct volume_geometry *geometry);

/**
 * Load the volume geometry from a layer.
 *
 * @param layer     The layer to read and parse the geometry from
 * @param geometry  The structure to receive the decoded fields
 **/
int __must_check
vdo_load_volume_geometry(PhysicalLayer *layer,
			 struct volume_geometry *geometry);

/**
 * Initialize a volume_geometry for a VDO.
 *
 * @param nonce         The nonce for the VDO
 * @param uuid          The uuid for the VDO
 * @param index_config  The index config of the VDO
 * @param geometry      The geometry being initialized
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check
vdo_initialize_volume_geometry(nonce_t nonce,
			       uuid_t *uuid,
			       const struct index_config *index_config,
			       struct volume_geometry *geometry);

/**
 * Zero out the geometry on a layer.
 *
 * @param layer  The layer to clear
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check vdo_clear_volume_geometry(PhysicalLayer *layer);

/**
 * Write a geometry block for a VDO.
 *
 * @param layer     The layer on which to write
 * @param geometry  The volume_geometry to be written
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check
vdo_write_volume_geometry(PhysicalLayer *layer,
			  struct volume_geometry *geometry);

/**
 * Write a specific version of geometry block for a VDO.
 *
 * @param layer     The layer on which to write
 * @param geometry  The VolumeGeometry to be written
 * @param version   The version of VolumeGeometry to write
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check
vdo_write_volume_geometry_with_version(PhysicalLayer *layer,
				       struct volume_geometry *geometry,
				       uint32_t version);

/**
 * Convert an index config to a UDS configuration, which can be used by UDS.
 *
 * @param index_config    The index config to convert
 * @param uds_config_ptr  A pointer to return the UDS configuration
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check
vdo_index_config_to_uds_configuration(const struct index_config *index_config,
				      struct uds_configuration **uds_config_ptr);

/**
 * Modify the uds_parameters to match the requested index config.
 *
 * @param index_config  The index config to convert
 * @param user_params   The uds_parameters to modify
 **/
void vdo_index_config_to_uds_parameters(const struct index_config *index_config,
					struct uds_parameters *user_params);

/**
 * Compute the index size in blocks from the index_config.
 *
 * @param index_config      The index config
 * @param index_blocks_ptr  A pointer to return the index size in blocks
 *
 * @return VDO_SUCCESS or an error
 **/
int __must_check vdo_compute_index_blocks(const struct index_config *index_config,
					  block_count_t *index_blocks_ptr);

#endif // VOLUME_GEOMETRY_H
