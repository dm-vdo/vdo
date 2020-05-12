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
 * $Id: //eng/uds-releases/krusty/src/uds/ioFactory.h#5 $
 */

#ifndef IO_FACTORY_H
#define IO_FACTORY_H

#include "bufferedReader.h"
#include "bufferedWriter.h"
#ifdef __KERNEL__
#include <linux/dm-bufio.h>
#else
#include "fileUtils.h"
#include "ioRegion.h"
#endif

/*
 * An IO factory object is responsible for controlling access to index
 * storage.  The index is a contiguous range of blocks on a block
 * device or within a file.
 *
 * The IO factory holds the open device or file and is responsible for
 * closing it.  The IO factory has methods to make IO regions that are
 * used to access sections of the index.
 */
struct io_factory;

/*
 * Define the UDS block size as 4K.  Historically, we wrote the volume file in
 * large blocks, but wrote all the other index data into byte streams stored in
 * files.  When we converted to writing an index into a block device, we
 * changed to writing the byte streams into page sized blocks.  Now that we
 * support multiple architectures, we write into 4K blocks on all platforms.
 *
 * XXX We must convert all the rogue 4K constants to use UDS_BLOCK_SIZE.
 */
enum { UDS_BLOCK_SIZE = 4096 };

#ifdef __KERNEL__
/**
 * Create an IO factory. The IO factory is returned with a reference
 * count of 1.
 *
 * @param path        The path to the block device or file that contains the
 *                    block stream
 * @param factory_ptr The IO factory is returned here
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check make_io_factory(const char *path,
				 struct io_factory **factory_ptr);
#else
/**
 * Create an IO factory.  The IO factory is returned with a reference
 * count of 1.
 *
 * @param path        The path to the block device or file that contains the
 *                    block stream
 * @param access      The requested access kind.
 * @param factory_ptr The IO factory is returned here
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check make_io_factory(const char *path,
				 FileAccess access,
				 struct io_factory **factory_ptr);
#endif

/**
 * Get another reference to an IO factory, incrementing its reference count.
 *
 * @param factory  The IO factory
 **/
void get_io_factory(struct io_factory *factory);

/**
 * Free a reference to an IO factory.  If the reference count drops to zero,
 * free the IO factory and release all its resources.
 *
 * @param factory  The IO factory
 **/
void put_io_factory(struct io_factory *factory);

/**
 * Get the maximum potential size of the device or file.  For a device, this is
 * the actual size of the device.  For a file, this is the largest file that we
 * can possibly write.
 *
 * @param factory  The IO factory
 *
 * @return the writable size (in bytes)
 **/
size_t __must_check get_writable_size(struct io_factory *factory);

#ifdef __KERNEL__
/**
 * Create a struct dm_bufio_client for a region of the index.
 *
 * @param factory          The IO factory
 * @param offset           The byte offset to the region within the index
 * @param block_size       The size of a block, in bytes
 * @param reserved_buffers The number of buffers that can be reserved
 * @param client_ptr       The struct dm_bufio_client is returned here
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check make_bufio(struct io_factory *factory,
			    off_t offset,
			    size_t block_size,
			    unsigned int reserved_buffers,
			    struct dm_bufio_client **client_ptr);
#else
/**
 * Create an IO region for a region of the index.
 *
 * @param factory    The IO factory
 * @param offset     The byte offset to the region within the index
 * @param size       The size in bytes of the region
 * @param region_ptr The IO region is returned here
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check make_io_region(struct io_factory *factory,
				off_t offset,
				size_t size,
				struct io_region **region_ptr);
#endif

/**
 * Create a BufferedReader for a region of the index.
 *
 * @param factory    The IO factory
 * @param offset     The byte offset to the region within the index
 * @param size       The size in bytes of the region
 * @param reader_ptr The BufferedReader is returned here
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check open_buffered_reader(struct io_factory *factory,
				      off_t offset,
				      size_t size,
				      BufferedReader **reader_ptr);

/**
 * Create a BufferedWriter for a region of the index.
 *
 * @param factory    The IO factory
 * @param offset     The byte offset to the region within the index
 * @param size       The size in bytes of the region
 * @param writer_ptr The BufferedWriter is returned here
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check open_buffered_writer(struct io_factory *factory,
				      off_t offset,
				      size_t size,
				      BufferedWriter **writer_ptr);

#endif // IO_FACTORY_H
