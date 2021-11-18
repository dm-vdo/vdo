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

#include "atomicDefs.h"
#include "fileIORegion.h"
#include "io-factory.h"
#include "memory-alloc.h"

/*
 * A user mode IOFactory object controls access to an index stored in a file.
 */
struct io_factory {
	int fd;
	atomic_t ref_count;
};

/**********************************************************************/
void get_uds_io_factory(struct io_factory *factory)
{
	atomic_inc(&factory->ref_count);
}

/**********************************************************************/
int make_uds_io_factory(const char *path,
			enum file_access access,
			struct io_factory **factory_ptr)
{
	struct io_factory *factory;
	int result = UDS_ALLOCATE(1, struct io_factory, __func__, &factory);
	if (result != UDS_SUCCESS) {
		return result;
	}

	result = open_file(path, access, &factory->fd);
	if (result != UDS_SUCCESS) {
		UDS_FREE(factory);
		return result;
	}

	atomic_set_release(&factory->ref_count, 1);
	*factory_ptr = factory;
	return UDS_SUCCESS;
}

/**********************************************************************/
void put_uds_io_factory(struct io_factory *factory)
{
	if (atomic_add_return(-1, &factory->ref_count) <= 0) {
		close_file(factory->fd, NULL);
		UDS_FREE(factory);
	}
}

/**********************************************************************/
size_t get_uds_writable_size(struct io_factory *factory __attribute__((unused)))
{
	/*
	 * The actual maximum is dependent upon the type of filesystem, and the
	 * man pages tell us no way to determine what that maximum is.
	 * Fortunately, any attempt to write to a location that is too large
	 * will return an EFBIG error.
	 */
	return SIZE_MAX;
}

/**********************************************************************/
int make_uds_io_region(struct io_factory *factory,
		       off_t offset,
		       size_t size,
		       struct io_region  **region_ptr)
{
	return make_file_region(factory,
				factory->fd,
				FU_READ_WRITE,
				offset,
				size,
				region_ptr);
}

/**********************************************************************/

int open_uds_buffered_reader(struct io_factory *factory,
			     off_t offset,
			     size_t size,
			     struct buffered_reader **reader_ptr)
{
	struct io_region *region;
	int result = make_file_region(factory,
				      factory->fd,
				      FU_READ_WRITE,
				      offset,
				      size,
				      &region);
	if (result != UDS_SUCCESS) {
		return result;
	}
	result = make_buffered_reader(region, reader_ptr);
	put_io_region(region);
	return result;
}

/**********************************************************************/
int open_uds_buffered_writer(struct io_factory *factory,
			     off_t offset,
			     size_t size,
			     struct buffered_writer **writer_ptr)
{
	struct io_region *region;
	int result = make_file_region(factory,
				      factory->fd,
				      FU_READ_WRITE,
				      offset,
				      size,
				      &region);

	if (result != UDS_SUCCESS) {
		return result;
	}
	result = make_buffered_writer(region, writer_ptr);
	put_io_region(region);
	return result;
}
