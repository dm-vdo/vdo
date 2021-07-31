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
 * $Id: //eng/uds-releases/krusty/src/uds/volumeStore.c#15 $
 */

#include "geometry.h"
#include "indexLayout.h"
#include "logger.h"
#include "uds-error.h"
#include "volumeStore.h"


/**********************************************************************/
void close_volume_store(struct volume_store *volume_store)
{
	if (volume_store->vs_region != NULL) {
		put_io_region(volume_store->vs_region);
		volume_store->vs_region = NULL;
	}
}

/**********************************************************************/
void destroy_volume_page(struct volume_page *volume_page)
{
	UDS_FREE(volume_page->vp_data);
	volume_page->vp_data = NULL;
}

/**********************************************************************/
int initialize_volume_page(const struct geometry *geometry,
			   struct volume_page *volume_page)
{
	return UDS_ALLOCATE_IO_ALIGNED(geometry->bytes_per_page,
				       byte, __func__, &volume_page->vp_data);

}

/**********************************************************************/
int open_volume_store(struct volume_store *volume_store,
		      struct index_layout *layout,
		      unsigned int reserved_buffers __maybe_unused,
		      size_t bytes_per_page)
{
	volume_store->vs_bytes_per_page = bytes_per_page;
	return open_uds_volume_region(layout, &volume_store->vs_region);
}

/**********************************************************************/
void prefetch_volume_pages(const struct volume_store *vs __maybe_unused,
			   unsigned int physical_page __maybe_unused,
			   unsigned int page_count __maybe_unused)
{
	// Nothing to do in user mode
}

/**********************************************************************/
int prepare_to_write_volume_page(const struct volume_store *volume_store
				 __maybe_unused,
				 unsigned int physical_page __maybe_unused,
				 struct volume_page *volume_page
				 __maybe_unused)
{
	// Nothing to do in user mode
	return UDS_SUCCESS;
}

/**********************************************************************/
int read_volume_page(const struct volume_store *volume_store,
		     unsigned int physical_page,
		     struct volume_page *volume_page)
{
	off_t offset = (off_t) physical_page * volume_store->vs_bytes_per_page;
	int result = read_from_region(volume_store->vs_region,
				      offset,
				      get_page_data(volume_page),
				      volume_store->vs_bytes_per_page,
				      NULL);
	if (result != UDS_SUCCESS) {
		return uds_log_warning_strerror(result,
						"error reading physical page %u",
						physical_page);
	}
	return UDS_SUCCESS;
}

/**********************************************************************/
void release_volume_page(struct volume_page *volume_page __maybe_unused)
{
	// Nothing to do in user mode
}

/**********************************************************************/
void swap_volume_pages(struct volume_page *volume_page1,
		       struct volume_page *volume_page2)
{
	struct volume_page temp = *volume_page1;
	*volume_page1 = *volume_page2;
	*volume_page2 = temp;
}

/**********************************************************************/
int sync_volume_store(const struct volume_store *volume_store)
{
	int result = sync_region_contents(volume_store->vs_region);
	if (result != UDS_SUCCESS) {
		return uds_log_error_strerror(result,
					      "cannot sync chapter to volume");
	}
	return UDS_SUCCESS;
}

/**********************************************************************/
int write_volume_page(const struct volume_store *volume_store,
		      unsigned int physical_page,
		      struct volume_page *volume_page)
{
	off_t offset = (off_t) physical_page * volume_store->vs_bytes_per_page;
	return write_to_region(volume_store->vs_region,
			       offset,
			       get_page_data(volume_page),
			       volume_store->vs_bytes_per_page,
			       volume_store->vs_bytes_per_page);
}
