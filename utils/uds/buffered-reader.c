// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright Red Hat
 */

#include "buffered-reader.h"

#include "compiler.h"
#include "io-factory.h"
#include "logger.h"
#include "memory-alloc.h"
#include "numeric.h"

/*
 * Define sector_t, because the kernel uses it extensively. Note that
 * the use of #define means that even if a user mode include typedefs
 * sector_t, it will not affect this module.
 */
#define sector_t uint64_t

/*
 * The buffered reader allows efficient I/O for IO regions. The internal
 * buffer always reads aligned data from the underlying region.
 */
struct buffered_reader {
	/* Region to read from */
	struct io_region *region;
	/* Number of the current block */
	uint64_t block_number;
	/* Start of the buffer */
	byte *start;
	/* End of the data read from the buffer */
	byte *end;
};


/*
 * Make a new buffered reader.
 *
 * @param region      An IO region to read from
 * @param reader_ptr  The pointer to hold the newly allocated buffered reader
 *
 * @return UDS_SUCCESS or error code.
 */
int make_buffered_reader(struct io_region *region,
			 struct buffered_reader **reader_ptr)
{
	byte *data;
	struct buffered_reader *reader = NULL;
	int result;

	result = UDS_ALLOCATE_IO_ALIGNED(UDS_BLOCK_SIZE,
					 byte,
					 "buffered reader buffer",
					 &data);
	if (result != UDS_SUCCESS) {
		return result;
	}

	result = UDS_ALLOCATE(1,
			      struct buffered_reader,
			      "buffered reader",
			      &reader);
	if (result != UDS_SUCCESS) {
		UDS_FREE(data);
		return result;
	}

	*reader = (struct buffered_reader) {
		.region = region,
		.block_number = 0,
		.start = data,
		.end = NULL,
	};

	get_io_region(region);
	*reader_ptr = reader;
	return UDS_SUCCESS;
}

void free_buffered_reader(struct buffered_reader *reader)
{
	if (reader == NULL) {
		return;
	}

	put_io_region(reader->region);
	UDS_FREE(reader->start);
	UDS_FREE(reader);
}

static int position_reader(struct buffered_reader *reader,
			   sector_t block_number,
			   off_t offset)
{
	if ((reader->end == NULL) || (block_number != reader->block_number)) {
		int result = read_from_region(reader->region,
					      block_number * UDS_BLOCK_SIZE,
					      reader->start,
					      UDS_BLOCK_SIZE,
					      NULL);
		if (result != UDS_SUCCESS) {
			uds_log_warning_strerror(result,
						 "%s: read_from_region error",
						 __func__);
			return result;
		}
	}

	reader->block_number = block_number;
	reader->end = reader->start + offset;
	return UDS_SUCCESS;
}

static size_t bytes_remaining_in_read_buffer(struct buffered_reader *reader)
{
	return (reader->end == NULL ?
		0 :
		reader->start + UDS_BLOCK_SIZE - reader->end);
}

static int reset_reader(struct buffered_reader *reader)
{
	sector_t block_number;

	if (bytes_remaining_in_read_buffer(reader) > 0) {
		return UDS_SUCCESS;
	}

	block_number = reader->block_number;
	if (reader->end != NULL) {
		++block_number;
	}

	return position_reader(reader, block_number, 0);
}

/*
 * Retrieve data from a buffered reader, reading from the region when needed.
 *
 * @param reader  The buffered reader
 * @param data    The buffer to read data into
 * @param length  The length of the data to read
 *
 * @return UDS_SUCCESS or an error code
 */
int read_from_buffered_reader(struct buffered_reader *reader,
			      void *data,
			      size_t length)
{
	byte *dp = data;
	int result = UDS_SUCCESS;
	size_t chunk;

	while (length > 0) {
		result = reset_reader(reader);
		if (result != UDS_SUCCESS) {
			break;
		}

		chunk = min(length, bytes_remaining_in_read_buffer(reader));
		memcpy(dp, reader->end, chunk);
		length -= chunk;
		dp += chunk;
		reader->end += chunk;
	}

	if (((result == UDS_OUT_OF_RANGE) || (result == UDS_END_OF_FILE)) &&
	    (dp - (byte *) data > 0)) {
		result = UDS_SHORT_READ;
	}

	return result;
}

/*
 * Verify that the data currently in the buffer matches the required value.
 *
 * @param reader  The buffered reader
 * @param value   The value that must match the buffer contents
 * @param length  The length of the value that must match
 *
 * @return UDS_SUCCESS or UDS_CORRUPT_DATA if the value does not match
 *
 * @note If the value matches, the matching contents are consumed. However,
 *       if the match fails, any buffer contents are left as is.
 */
int verify_buffered_data(struct buffered_reader *reader,
			 const void *value,
			 size_t length)
{
	int result = UDS_SUCCESS;
	size_t chunk;
	const byte *vp = value;
	sector_t start_block_number = reader->block_number;
	int start_offset = reader->end - reader->start;

	while (length > 0) {
		result = reset_reader(reader);
		if (result != UDS_SUCCESS) {
			result = UDS_CORRUPT_DATA;
			break;
		}

		chunk = min(length, bytes_remaining_in_read_buffer(reader));
		if (memcmp(vp, reader->end, chunk) != 0) {
			result = UDS_CORRUPT_DATA;
			break;
		}

		length -= chunk;
		vp += chunk;
		reader->end += chunk;
	}

	if (result != UDS_SUCCESS) {
		position_reader(reader, start_block_number, start_offset);
	}

	return result;
}
