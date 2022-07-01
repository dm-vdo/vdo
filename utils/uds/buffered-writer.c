// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright Red Hat
 */

#include "buffered-writer.h"

#include "compiler.h"
#include "errors.h"
#include "io-factory.h"
#include "logger.h"
#include "memory-alloc.h"
#include "numeric.h"

struct buffered_writer {
	/* Region to write to */
	struct io_region *region;
	/* Number of the current block */
	uint64_t block_number;
	/* Start of the buffer */
	byte *start;
	/* End of the data written to the buffer */
	byte *end;
	/* Error code */
	int error;
};

static INLINE size_t space_used_in_buffer(struct buffered_writer *writer)
{
	return writer->end - writer->start;
}

static
size_t space_remaining_in_write_buffer(struct buffered_writer *writer)
{
	return UDS_BLOCK_SIZE - space_used_in_buffer(writer);
}


/*
 * Make a new buffered writer.
 *
 * @param region      The IO region to write to
 * @param writer_ptr  The new buffered writer goes here
 *
 * @return UDS_SUCCESS or an error code
 */
int make_buffered_writer(struct io_region *region,
			 struct buffered_writer **writer_ptr)
{
	int result;
	byte *data;
	struct buffered_writer *writer;

	result = UDS_ALLOCATE_IO_ALIGNED(UDS_BLOCK_SIZE,
					 byte,
					 "buffered writer buffer",
					 &data);
	if (result != UDS_SUCCESS) {
		return result;
	}

	result = UDS_ALLOCATE(1,
			      struct buffered_writer,
			      "buffered writer",
			      &writer);
	if (result != UDS_SUCCESS) {
		UDS_FREE(data);
		return result;
	}

	*writer = (struct buffered_writer) {
		.region = region,
		.start = data,
		.end = data,
		.block_number = 0,
		.error = UDS_SUCCESS,
	};

	get_io_region(region);
	*writer_ptr = writer;
	return UDS_SUCCESS;
}

void free_buffered_writer(struct buffered_writer *writer)
{
	int result;

	if (writer == NULL) {
		return;
	}

	result = sync_region_contents(writer->region);
	if (result != UDS_SUCCESS) {
		uds_log_warning_strerror(result,
					 "%s: failed to sync storage",
					 __func__);
	}

	put_io_region(writer->region);
	UDS_FREE(writer->start);
	UDS_FREE(writer);
}

/*
 * Append data to the buffer, writing as needed. If a write error occurs, it
 * is recorded and returned on every subsequent write attempt.
 */
int write_to_buffered_writer(struct buffered_writer *writer,
			     const void *data,
			     size_t len)
{
	const byte *dp = data;
	int result = UDS_SUCCESS;
	size_t chunk;

	if (writer->error != UDS_SUCCESS) {
		return writer->error;
	}

	while ((len > 0) && (result == UDS_SUCCESS)) {
		chunk = min(len, space_remaining_in_write_buffer(writer));
		memcpy(writer->end, dp, chunk);
		len -= chunk;
		dp += chunk;
		writer->end += chunk;

		if (space_remaining_in_write_buffer(writer) == 0) {
			result = flush_buffered_writer(writer);
		}
	}

	return result;
}

int write_zeros_to_buffered_writer(struct buffered_writer *writer, size_t len)
{
	int result = UDS_SUCCESS;
	size_t chunk;

	if (writer->error != UDS_SUCCESS) {
		return writer->error;
	}

	while ((len > 0) && (result == UDS_SUCCESS)) {

		chunk = min(len, space_remaining_in_write_buffer(writer));
		memset(writer->end, 0, chunk);
		len -= chunk;
		writer->end += chunk;

		if (space_remaining_in_write_buffer(writer) == 0) {
			result = flush_buffered_writer(writer);
		}
	}

	return result;
}

int flush_buffered_writer(struct buffered_writer *writer)
{
	if (writer->error != UDS_SUCCESS) {
		return writer->error;
	}

	if (space_used_in_buffer(writer) == 0) {
		return UDS_SUCCESS;
	}

	writer->error = write_to_region(writer->region,
					writer->block_number * UDS_BLOCK_SIZE,
					writer->start,
					UDS_BLOCK_SIZE,
					space_used_in_buffer(writer));
	if (writer->error == UDS_SUCCESS) {
		writer->end = writer->start;
		writer->block_number++;
	}

	return writer->error;
}
