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
 * $Id: //eng/linux-vdo/src/c++/vdo/base/physicalLayer.h#59 $
 */

#ifndef PHYSICAL_LAYER_H
#define PHYSICAL_LAYER_H

#include "types.h"

typedef struct physicalLayer PhysicalLayer;

/**
 * A function to destroy a physical layer and NULL out the reference to it.
 *
 * @param layer_ptr  A pointer to the layer to destroy
 **/
typedef void layer_destructor(PhysicalLayer **layer_ptr);

/**
 * A function to report the block count of a physicalLayer.
 *
 * @param layer  The layer
 *
 * @return The block count of the layer
 **/
typedef block_count_t block_count_getter(PhysicalLayer *layer);

/**
 * A function which can allocate a buffer suitable for use in an
 * extent_reader or extent_writer.
 *
 * @param [in]  layer       The physical layer in question
 * @param [in]  bytes       The size of the buffer, in bytes.
 * @param [in]  why         The occasion for allocating the buffer
 * @param [out] buffer_ptr  A pointer to hold the buffer
 *
 * @return a success or error code
 **/
typedef int buffer_allocator(PhysicalLayer *layer,
			     size_t bytes,
			     const char *why,
			     char **buffer_ptr);

/**
 * A function which can read an extent from a physicalLayer.
 *
 * @param [in]  layer       The physical layer from which to read
 * @param [in]  startBlock  The physical block number of the start of the
 *                          extent
 * @param [in]  blockCount  The number of blocks in the extent
 * @param [out] buffer      A buffer to hold the extent
 *
 * @return a success or error code
 **/
typedef int extent_reader(PhysicalLayer *layer,
			  physical_block_number_t startBlock,
			  size_t blockCount,
			  char *buffer);

/**
 * A function which can write an extent to a physicalLayer.
 *
 * @param [in]  layer          The physical layer to which to write
 * @param [in]  startBlock     The physical block number of the start of the
 *                             extent
 * @param [in]  blockCount     The number of blocks in the extent
 * @param [in]  buffer         The buffer which contains the data
 *
 * @return a success or error code
 **/
typedef int extent_writer(PhysicalLayer *layer,
			  physical_block_number_t startBlock,
			  size_t blockCount,
			  char *buffer);

/**
 * An abstraction representing the underlying physical layer.
 **/
struct physicalLayer {
	// Management interface
	layer_destructor *destroy;

	// Synchronous interface
	block_count_getter *getBlockCount;

	// Synchronous IO interface
	buffer_allocator *allocateIOBuffer;
	extent_reader *reader;
	extent_writer *writer;

};

#endif // PHYSICAL_LAYER_H
