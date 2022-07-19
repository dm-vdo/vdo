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

#ifndef BLOCK_MAP_UTILS_H
#define BLOCK_MAP_UTILS_H

#include "block-map-page.h"

#include "physicalLayer.h"
#include "userVDO.h"

/**
 * A function which examines a block map page entry. Functions of this type are
 * passed to examineBlockMapPages() which will iterate over the entire block
 * map and call this function once for each non-empty mapping.
 *
 * @param slot        The block_map_slot where this entry was found
 * @param height      The height of the block map entry in the tree
 * @param pbn         The PBN encoded in the entry
 * @param state       The mapping state encoded in the entry
 *
 * @return VDO_SUCCESS or an error code
 **/
typedef int __must_check
MappingExaminer(struct block_map_slot slot,
		height_t height,
		physical_block_number_t pbn,
		enum block_mapping_state state);

/**
 * Apply a mapping examiner to each mapped block map entry in a VDO.
 *
 * @param vdo       The VDO containing the block map to be examined
 * @param examiner  The examiner to apply to each defined mapping
 *
 * @return VDO_SUCCESS or an error code
 **/
int __must_check
examineBlockMapEntries(UserVDO *vdo, MappingExaminer *examiner);

/**
 * Find the PBN for the block map page encoding a particular LBN mapping.
 * This will return the zero block if there is no mapping.
 *
 * @param [in]  vdo     The VDO
 * @param [in]  lbn     The logical block number to look up
 * @param [out] pbnPtr  A pointer to the PBN of the requested block map page
 *
 * @return VDO_SUCCESS or an error code
 **/
int __must_check findLBNPage(UserVDO *vdo,
			     logical_block_number_t lbn,
			     physical_block_number_t *pbnPtr);

/**
 * Look up the mapping for a single LBN in the block map.
 *
 * @param [in]  vdo       The VDO
 * @param [in]  lbn       The logical block number to look up
 * @param [out] pbnPtr    A pointer to the mapped PBN
 * @param [out] statePtr  A pointer to the mapping state
 *
 * @return VDO_SUCCESS or an error code
 **/
int __must_check findLBNMapping(UserVDO *vdo,
				logical_block_number_t lbn,
				physical_block_number_t *pbnPtr,
				enum block_mapping_state *statePtr);

/**
 * Read a single block map page into the buffer. The page will be marked
 * initialized iff the page is valid.
 *
 * @param [in]  layer  The layer from which to read the page
 * @param [in]  pbn    The absolute physical block number of the page to
 *                     read
 * @param [in]  nonce  The VDO nonce
 * @param [out] page   The page structure to read into
 *
 * @return VDO_SUCCESS or an error code
 **/
int __must_check readBlockMapPage(PhysicalLayer *layer,
				  physical_block_number_t pbn,
				  nonce_t nonce,
				  struct block_map_page *page);

#endif // BLOCK_MAP_UTILS_H
