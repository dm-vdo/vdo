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
 * $Id: //eng/uds-releases/krusty/src/uds/bufferedWriter.c#12 $
 */

#include "bufferedWriter.h"

#include "compiler.h"
#include "errors.h"
#include "ioFactory.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "numeric.h"


struct buffered_writer {
	// Region to write to
	struct io_region *bw_region;
	// Number of the current block
	uint64_t bw_block_number;
	// Start of the buffer
	byte *bw_start;
	// End of the data written to the buffer
	byte *bw_pointer;
	// Error code
	int bw_error;
	// Have writes been done?
	bool bw_used;
};


/**********************************************************************/
int make_buffered_writer(struct io_region *region,
			 struct buffered_writer **writer_ptr)
{
	byte *data;
	int result = ALLOCATE_IO_ALIGNED(UDS_BLOCK_SIZE, byte,
					 "buffer writer buffer", &data);
	if (result != UDS_SUCCESS) {
		return result;
	}

	struct buffered_writer *writer;
	result =
		ALLOCATE(1, struct buffered_writer, "buffered writer", &writer);
	if (result != UDS_SUCCESS) {
		FREE(data);
		return result;
	}

	*writer = (struct buffered_writer){
		.bw_region = region,
		.bw_start = data,
		.bw_pointer = data,
		.bw_block_number = 0,
		.bw_error = UDS_SUCCESS,
		.bw_used = false,
	};

	get_io_region(region);
	*writer_ptr = writer;
	return UDS_SUCCESS;
}

/**********************************************************************/
void free_buffered_writer(struct buffered_writer *bw)
{
	int result;
	if (bw == NULL) {
		return;
	}
	result = sync_region_contents(bw->bw_region);
	if (result != UDS_SUCCESS) {
		log_warning_strerror(result,
				     "%s cannot sync storage", __func__);
			
	}
	put_io_region(bw->bw_region);
	FREE(bw->bw_start);
	FREE(bw);
}

/**********************************************************************/
static INLINE size_t space_used_in_buffer(struct buffered_writer *bw)
{
	return bw->bw_pointer - bw->bw_start;
}

/**********************************************************************/
size_t space_remaining_in_write_buffer(struct buffered_writer *bw)
{
	return UDS_BLOCK_SIZE - space_used_in_buffer(bw);
}

/**********************************************************************/
int write_to_buffered_writer(struct buffered_writer *bw,
			     const void *data,
			     size_t len)
{
	const byte *dp = data;
	int result = UDS_SUCCESS;
	size_t avail, chunk;
	if (bw->bw_error != UDS_SUCCESS) {
		return bw->bw_error;
	}

	while ((len > 0) && (result == UDS_SUCCESS)) {

		avail = space_remaining_in_write_buffer(bw);
		chunk = min(len, avail);
		memcpy(bw->bw_pointer, dp, chunk);
		len -= chunk;
		dp += chunk;
		bw->bw_pointer += chunk;

		if (space_remaining_in_write_buffer(bw) == 0) {
			result = flush_buffered_writer(bw);
		}
	}

	bw->bw_used = true;
	return result;
}

/**********************************************************************/
int write_zeros_to_buffered_writer(struct buffered_writer *bw, size_t len)
{
	int result = UDS_SUCCESS;
	size_t avail, chunk;
	if (bw->bw_error != UDS_SUCCESS) {
		return bw->bw_error;
	}

	while ((len > 0) && (result == UDS_SUCCESS)) {

		avail = space_remaining_in_write_buffer(bw);
		chunk = min(len, avail);
		memset(bw->bw_pointer, 0, chunk);
		len -= chunk;
		bw->bw_pointer += chunk;

		if (space_remaining_in_write_buffer(bw) == 0) {
			result = flush_buffered_writer(bw);
		}
	}

	bw->bw_used = true;
	return result;
}

/**********************************************************************/
int flush_buffered_writer(struct buffered_writer *bw)
{
	if (bw->bw_error != UDS_SUCCESS) {
		return bw->bw_error;
	}

	size_t n = space_used_in_buffer(bw);
	if (n > 0) {
		int result =
			write_to_region(bw->bw_region,
					bw->bw_block_number * UDS_BLOCK_SIZE,
					bw->bw_start,
					UDS_BLOCK_SIZE,
					n);
		if (result != UDS_SUCCESS) {
			return bw->bw_error = result;
		} else {
			bw->bw_pointer = bw->bw_start;
			bw->bw_block_number++;
		}
	}
	return UDS_SUCCESS;
}

/**********************************************************************/
bool was_buffered_writer_used(const struct buffered_writer *bw)
{
	return bw->bw_used;
}

/**********************************************************************/
void note_buffered_writer_used(struct buffered_writer *bw)
{
	bw->bw_used = true;
}
