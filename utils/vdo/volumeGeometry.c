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
 * $Id: //eng/linux-vdo/src/c++/vdo/base/volumeGeometry.c#39 $
 */

#include "volumeGeometry.h"

#include "buffer.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "numeric.h"
#include "permassert.h"

#include "checksum.h"
#include "constants.h"
#include "header.h"
#include "physicalLayer.h"
#include "releaseVersions.h"
#include "statusCodes.h"
#include "types.h"

enum {
	MAGIC_NUMBER_SIZE = 8,
};

struct geometry_block {
	char magic_number[MAGIC_NUMBER_SIZE];
	struct header header;
	struct volume_geometry geometry;
	crc32_checksum_t checksum;
} __packed;

static const struct header GEOMETRY_BLOCK_HEADER_4_0 = {
	.id = GEOMETRY_BLOCK,
	.version = {
		.major_version = 4,
		.minor_version = 0,
	},
	// Note: this size isn't just the payload size following the header,
	// like it is everywhere else in VDO.
	.size = sizeof(struct geometry_block),
};

static const byte MAGIC_NUMBER[MAGIC_NUMBER_SIZE + 1] = "dmvdo001";

static const release_version_number_t COMPATIBLE_RELEASE_VERSIONS[] = {
	MAGNESIUM_RELEASE_VERSION_NUMBER,
	ALUMINUM_RELEASE_VERSION_NUMBER,
};

/**
 * Determine whether the supplied release version can be understood by
 * the VDO code.
 *
 * @param version  The release version number to check
 *
 * @return <code>True</code> if the given version can be loaded.
 **/
static inline bool is_loadable_release_version(release_version_number_t version)
{
	unsigned int i;
	if (version == CURRENT_RELEASE_VERSION_NUMBER) {
		return true;
	}

	for (i = 0; i < COUNT_OF(COMPATIBLE_RELEASE_VERSIONS); i++) {
		if (version == COMPATIBLE_RELEASE_VERSIONS[i]) {
			return true;
		}
	}

	return false;
}

/**
 * Decode the on-disk representation of an index configuration from a buffer.
 *
 * @param buffer  A buffer positioned at the start of the encoding
 * @param config  The structure to receive the decoded fields
 *
 * @return UDS_SUCCESS or an error
 **/
static int decode_index_config(struct buffer *buffer,
			       struct index_config *config)
{
	uint32_t mem;
	uint32_t checkpoint_frequency;
	bool sparse;
	int result = get_uint32_le_from_buffer(buffer, &mem);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = get_uint32_le_from_buffer(buffer, &checkpoint_frequency);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = get_boolean(buffer, &sparse);
	if (result != VDO_SUCCESS) {
		return result;
	}

	*config = (struct index_config) {
		.mem = mem,
		.checkpoint_frequency = checkpoint_frequency,
		.sparse = sparse,
	};
	return VDO_SUCCESS;
}

/**
 * Encode the on-disk representation of an index configuration into a buffer.
 *
 * @param config  The index configuration to encode
 * @param buffer  A buffer positioned at the start of the encoding
 *
 * @return UDS_SUCCESS or an error
 **/
static int encode_index_config(const struct index_config *config,
			       struct buffer *buffer)
{
	int result = put_uint32_le_into_buffer(buffer, config->mem);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = put_uint32_le_into_buffer(buffer, config->checkpoint_frequency);
	if (result != VDO_SUCCESS) {
		return result;
	}

	return put_boolean(buffer, config->sparse);
}

/**
 * Decode the on-disk representation of a volume region from a buffer.
 *
 * @param buffer  A buffer positioned at the start of the encoding
 * @param region  The structure to receive the decoded fields
 *
 * @return UDS_SUCCESS or an error
 **/
static int decode_volume_region(struct buffer *buffer,
				struct volume_region *region)
{
	physical_block_number_t start_block;
	enum volume_region_id id;
	int result = get_uint32_le_from_buffer(buffer, &id);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = get_uint64_le_from_buffer(buffer, &start_block);
	if (result != VDO_SUCCESS) {
		return result;
	}

	*region = (struct volume_region) {
		.id = id,
		.start_block = start_block,
	};
	return VDO_SUCCESS;
}

/**
 * Encode the on-disk representation of a volume region into a buffer.
 *
 * @param region  The region to encode
 * @param buffer  A buffer positioned at the start of the encoding
 *
 * @return UDS_SUCCESS or an error
 **/
static int encode_volume_region(const struct volume_region *region,
				struct buffer *buffer)
{
	int result = put_uint32_le_into_buffer(buffer, region->id);
	if (result != VDO_SUCCESS) {
		return result;
	}

	return put_uint64_le_into_buffer(buffer, region->start_block);
}

/**
 * Decode the on-disk representation of a volume geometry from a buffer.
 *
 * @param buffer    A buffer positioned at the start of the encoding
 * @param geometry  The structure to receive the decoded fields
 *
 * @return UDS_SUCCESS or an error
 **/
static int decode_volume_geometry(struct buffer *buffer,
				  struct volume_geometry *geometry)
{
	release_version_number_t release_version;
	enum volume_region_id id;
	nonce_t nonce;
	int result = get_uint32_le_from_buffer(buffer, &release_version);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = get_uint64_le_from_buffer(buffer, &nonce);
	if (result != VDO_SUCCESS) {
		return result;
	}

	geometry->release_version = release_version;
	geometry->nonce = nonce;

	result = get_bytes_from_buffer(buffer, sizeof(uuid_t),
				       (unsigned char *) &geometry->uuid);
	if (result != VDO_SUCCESS) {
		return result;
	}

	for (id = 0; id < VOLUME_REGION_COUNT; id++) {
		result = decode_volume_region(buffer, &geometry->regions[id]);
		if (result != VDO_SUCCESS) {
			return result;
		}
	}

	return decode_index_config(buffer, &geometry->index_config);
}

/**
 * Encode the on-disk representation of a volume geometry into a buffer.
 *
 * @param geometry  The geometry to encode
 * @param buffer    A buffer positioned at the start of the encoding
 *
 * @return UDS_SUCCESS or an error
 **/
static int encode_volume_geometry(const struct volume_geometry *geometry,
				  struct buffer *buffer)
{
	enum volume_region_id id;
	int result = put_uint32_le_into_buffer(buffer, geometry->release_version);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = put_uint64_le_into_buffer(buffer, geometry->nonce);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = put_bytes(buffer, sizeof(uuid_t),
			   (unsigned char *) &geometry->uuid);
	if (result != VDO_SUCCESS) {
		return result;
	}

	for (id = 0; id < VOLUME_REGION_COUNT; id++) {
		result = encode_volume_region(&geometry->regions[id], buffer);
		if (result != VDO_SUCCESS) {
			return result;
		}
	}

	return encode_index_config(&geometry->index_config, buffer);
}

/**
 * Decode the on-disk representation of a geometry block, up to but not
 * including the checksum, from a buffer.
 *
 * @param buffer    A buffer positioned at the start of the block
 * @param geometry  The structure to receive the decoded volume geometry fields
 *
 * @return UDS_SUCCESS or an error
 **/
static int decode_geometry_block(struct buffer *buffer,
				 struct volume_geometry *geometry)
{
	int result;
	struct header header;

	if (!has_same_bytes(buffer, MAGIC_NUMBER, MAGIC_NUMBER_SIZE)) {
		return VDO_BAD_MAGIC;
	}

	result = skip_forward(buffer, MAGIC_NUMBER_SIZE);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = decode_vdo_header(buffer, &header);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = validate_vdo_header(&GEOMETRY_BLOCK_HEADER_4_0, &header,
				     true, __func__);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = decode_volume_geometry(buffer, geometry);
	if (result != VDO_SUCCESS) {
		return result;
	}

	// Leave the CRC for the caller to decode and verify.
	return ASSERT(header.size == (uncompacted_amount(buffer) +
				      sizeof(crc32_checksum_t)),
		      "should have decoded up to the geometry checksum");
}

/**
 * Encode the on-disk representation of a geometry block, up to but not
 * including the checksum, into a buffer.
 *
 * @param geometry  The volume geometry to encode into the block
 * @param buffer    A buffer positioned at the start of the block
 *
 * @return UDS_SUCCESS or an error
 **/
static int encode_geometry_block(const struct volume_geometry *geometry,
				 struct buffer *buffer)
{
	int result = put_bytes(buffer, MAGIC_NUMBER_SIZE, MAGIC_NUMBER);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = encode_vdo_header(&GEOMETRY_BLOCK_HEADER_4_0, buffer);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = encode_volume_geometry(geometry, buffer);
	if (result != VDO_SUCCESS) {
		return result;
	}

	// Leave the CRC for the caller to compute and encode.
	return ASSERT(GEOMETRY_BLOCK_HEADER_4_0.size ==
			      (content_length(buffer) +
			       sizeof(crc32_checksum_t)),
		      "should have decoded up to the geometry checksum");
}

/**
 * Allocate a block-size buffer to read the geometry from the physical layer,
 * read the block, and return the buffer.
 *
 * @param [in]  layer      The physical layer containing the block to read
 * @param [out] block_ptr  A pointer to receive the allocated buffer
 *
 * @return VDO_SUCCESS or an error code
 **/
static int __must_check
read_geometry_block(PhysicalLayer *layer, byte **block_ptr)
{
	char *block;
	int result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
					     "geometry block", &block);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = layer->reader(layer, GEOMETRY_BLOCK_LOCATION, 1, block);
	if (result != VDO_SUCCESS) {
		FREE(block);
		return result;
	}

	*block_ptr = (byte *) block;
	return VDO_SUCCESS;
}

/**********************************************************************/
int load_volume_geometry(PhysicalLayer *layer, struct volume_geometry *geometry)
{
	byte *block;
	int result = read_geometry_block(layer, &block);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = parse_geometry_block(block, geometry);
	FREE(block);
	return result;
}

/**********************************************************************/
int parse_geometry_block(byte *block, struct volume_geometry *geometry)
{
	crc32_checksum_t checksum, saved_checksum;
	struct buffer *buffer;
	int result;

	result = wrap_buffer(block, VDO_BLOCK_SIZE, VDO_BLOCK_SIZE, &buffer);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = decode_geometry_block(buffer, geometry);
	if (result != VDO_SUCCESS) {
		free_buffer(&buffer);
		return result;
	}

	// Checksum everything decoded so far.
	checksum = update_crc32(INITIAL_CHECKSUM, block,
				uncompacted_amount(buffer));
	result = get_uint32_le_from_buffer(buffer, &saved_checksum);
	if (result != VDO_SUCCESS) {
		free_buffer(&buffer);
		return result;
	}

	// Finished all decoding. Everything that follows is validation code.
	free_buffer(&buffer);

	if (!is_loadable_release_version(geometry->release_version)) {
		return log_error_strerror(VDO_UNSUPPORTED_VERSION,
					  "release version %d cannot be loaded",
					  geometry->release_version);
	}

	return ((checksum == saved_checksum) ? VDO_SUCCESS :
					      VDO_CHECKSUM_MISMATCH);
}

/************************************************************************/
int compute_index_blocks(const struct index_config *index_config,
			 block_count_t *index_blocks_ptr)
{
	uint64_t index_bytes;
	block_count_t index_blocks;
	struct uds_configuration *uds_configuration = NULL;
	int result = index_config_to_uds_configuration(index_config,
						       &uds_configuration);
	if (result != UDS_SUCCESS) {
		return log_error_strerror(result,
					  "error creating index config");
	}

	result = uds_compute_index_size(uds_configuration, 0, &index_bytes);
	uds_free_configuration(uds_configuration);
	if (result != UDS_SUCCESS) {
		return log_error_strerror(result,
					  "error computing index size");
	}

	index_blocks = index_bytes / VDO_BLOCK_SIZE;
	if ((((uint64_t) index_blocks) * VDO_BLOCK_SIZE) != index_bytes) {
		return log_error_strerror(VDO_PARAMETER_MISMATCH,
					  "index size must be a multiple of block size %d",
					  VDO_BLOCK_SIZE);
	}

	*index_blocks_ptr = index_blocks;
	return VDO_SUCCESS;
}

/**********************************************************************/
int initialize_volume_geometry(nonce_t nonce,
			       uuid_t *uuid,
			       const struct index_config *index_config,
			       struct volume_geometry *geometry)
{
	block_count_t index_size = 0;
	if (index_config != NULL) {
		int result = compute_index_blocks(index_config, &index_size);
		if (result != VDO_SUCCESS) {
			return result;
		}
	}

	*geometry = (struct volume_geometry) {
		.release_version = CURRENT_RELEASE_VERSION_NUMBER,
		.nonce = nonce,
		.regions = {
			[INDEX_REGION] = {
				.id = INDEX_REGION,
				.start_block = 1,
			},
			[DATA_REGION] = {
				.id = DATA_REGION,
				.start_block = 1 + index_size,
			}
		}
	};
	uuid_copy(geometry->uuid, *uuid);
	if (index_size > 0) {
		memcpy(&geometry->index_config,
		       index_config,
		       sizeof(struct index_config));
	}

	return VDO_SUCCESS;
}

/**********************************************************************/
int clear_volume_geometry(PhysicalLayer *layer)
{
	char *block;
	int result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
					     "geometry block", &block);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = layer->writer(layer, GEOMETRY_BLOCK_LOCATION, 1, block);
	FREE(block);
	return result;
}

/**********************************************************************/
int write_volume_geometry(PhysicalLayer *layer,
			  struct volume_geometry *geometry)
{
	char *block;
	struct buffer *buffer;
	crc32_checksum_t checksum;

	int result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
					     "geometry block", &block);
	if (result != VDO_SUCCESS) {
		return result;
	}

	result = wrap_buffer((byte *) block, VDO_BLOCK_SIZE, 0, &buffer);
	if (result != VDO_SUCCESS) {
		FREE(block);
		return result;
	}

	result = encode_geometry_block(geometry, buffer);
	if (result != VDO_SUCCESS) {
		free_buffer(&buffer);
		FREE(block);
		return result;
	}

	// Checksum everything encoded so far and then encode the checksum.
	checksum = update_crc32(INITIAL_CHECKSUM, (byte *) block,
				content_length(buffer));
	result = put_uint32_le_into_buffer(buffer, checksum);
	if (result != VDO_SUCCESS) {
		free_buffer(&buffer);
		FREE(block);
		return result;
	}

	// Write it.
	result = layer->writer(layer, GEOMETRY_BLOCK_LOCATION, 1, block);
	free_buffer(&buffer);
	FREE(block);
	return result;
}

/************************************************************************/
int index_config_to_uds_configuration(const struct index_config *index_config,
				      struct uds_configuration **uds_config_ptr)
{
	struct uds_configuration *uds_configuration;
	int result = uds_initialize_configuration(&uds_configuration,
						  index_config->mem);
	if (result != UDS_SUCCESS) {
		return log_error_strerror(result,
					  "error initializing configuration");
	}

	uds_configuration_set_sparse(uds_configuration, index_config->sparse);
	*uds_config_ptr = uds_configuration;
	return VDO_SUCCESS;
}

/************************************************************************/
void index_config_to_uds_parameters(const struct index_config *index_config,
				    struct uds_parameters *user_params)
{
	user_params->checkpoint_frequency = index_config->checkpoint_frequency;
}
