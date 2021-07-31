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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/fileIORegion.c#13 $
 */

#include "fileIORegion.h"

#include "compiler.h"
#include "ioFactory.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"


struct file_io_region {
	struct io_region common;
	struct io_factory *factory;
	int fd;
	bool reading;
	bool writing;
	off_t offset;
	size_t size;
};

/**********************************************************************/
static INLINE struct file_io_region *as_file_io_region(struct io_region *region)
{
	return container_of(region, struct file_io_region, common);
}

/**********************************************************************/
static int validate_io(struct file_io_region *fior,
		       off_t offset,
		       size_t size,
		       size_t length,
		       bool will_write)
{
	if (!(will_write ? fior->writing : fior->reading)) {
		return uds_log_error_strerror(UDS_BAD_IO_DIRECTION,
					      "not open for %s",
					      will_write ? "writing" :
					      "reading");
	}

	if (length > size) {
		return uds_log_error_strerror(UDS_BUFFER_ERROR,
					      "length %zd exceeds buffer size %zd",
					      length,
					      size);
	}

	if (offset + length > fior->size) {
		return uds_log_error_strerror(UDS_OUT_OF_RANGE,
					      "range %lld-%lld not in range 0 to %zu",
					      (long long)offset,
					      (long long)offset + length,
					      fior->size);
	}

	return UDS_SUCCESS;
}

/**********************************************************************/
static void fior_free(struct io_region *region)
{
	struct file_io_region *fior = as_file_io_region(region);
	put_uds_io_factory(fior->factory);
	UDS_FREE(fior);
}

/**********************************************************************/
static int fior_write(struct io_region *region,
		      off_t offset,
		      const void *data,
		      size_t size,
		      size_t length)
{
	struct file_io_region *fior = as_file_io_region(region);


	int result = validate_io(fior, offset, size, length, true);
	if (result != UDS_SUCCESS) {
		return result;
	}

	return write_buffer_at_offset(fior->fd, fior->offset + offset, data,
				      length);
}

/**********************************************************************/
static int fior_read(struct io_region *region,
		     off_t offset,
		     void *buffer,
		     size_t size,
		     size_t *length)
{
	struct file_io_region *fior = as_file_io_region(region);

	size_t len = (length == NULL) ? size : *length;

	int result = validate_io(fior, offset, size, len, false);
	if (result != UDS_SUCCESS) {
		return result;
	}

	size_t data_length = 0;
	result = read_data_at_offset(fior->fd, fior->offset + offset, buffer,
				     size, &data_length);
	if (result != UDS_SUCCESS) {
		return result;
	}
	if (length == NULL) {
		if (data_length < size) {
			byte *buf = buffer;
			memset(&buf[data_length], 0, size - data_length);
		}
		return result;
	}

	if (data_length < *length) {
		if (data_length == 0) {
			return uds_log_error_strerror(UDS_END_OF_FILE,
						      "expected at least %zu bytes, got EOF",
						      len);
		} else {
			return uds_log_error_strerror(UDS_SHORT_READ,
						      "expected at least %zu bytes, got %zu",
						      len,
						      data_length);
		}
	}
	*length = data_length;
	return UDS_SUCCESS;
}

/**********************************************************************/
static int fior_sync_contents(struct io_region *region)
{
	struct file_io_region *fior = as_file_io_region(region);
	return logging_fsync(fior->fd,
			     "cannot sync contents of file IO region");
}

/**********************************************************************/
int make_file_region(struct io_factory *factory,
		     int fd,
		     enum file_access access,
		     off_t offset,
		     size_t size,
		     struct io_region **region_ptr)
{
	struct file_io_region *fior;
	int result = UDS_ALLOCATE(1, struct file_io_region, __func__, &fior);
	if (result != UDS_SUCCESS) {
		return result;
	}

	get_uds_io_factory(factory);

	fior->common.free = fior_free;
	fior->common.read = fior_read;
	fior->common.sync_contents = fior_sync_contents;
	fior->common.write = fior_write;
	fior->factory = factory;
	fior->fd = fd;
	fior->reading = (access <= FU_CREATE_READ_WRITE);
	fior->writing = (access >= FU_READ_WRITE);
	fior->offset = offset;
	fior->size = size;

	atomic_set_release(&fior->common.ref_count, 1);
	*region_ptr = &fior->common;
	return UDS_SUCCESS;
}
