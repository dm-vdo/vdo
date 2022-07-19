// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright Red Hat
 */

#include "geometry.h"
#include "index-layout.h"
#include "logger.h"
#include "volume-store.h"


void close_volume_store(struct volume_store *volume_store)
{
	if (volume_store->vs_region != NULL) {
		put_io_region(volume_store->vs_region);
		volume_store->vs_region = NULL;
	}
}

void destroy_volume_page(struct volume_page *volume_page)
{
	UDS_FREE(volume_page->vp_data);
	volume_page->vp_data = NULL;
}

int initialize_volume_page(size_t page_size, struct volume_page *volume_page)
{
	return UDS_ALLOCATE_IO_ALIGNED(page_size,
				       byte,
				       __func__,
				       &volume_page->vp_data);
}

int open_volume_store(struct volume_store *volume_store,
		      struct index_layout *layout,
		      unsigned int reserved_buffers __maybe_unused,
		      size_t bytes_per_page)
{
	volume_store->vs_bytes_per_page = bytes_per_page;
	return open_uds_volume_region(layout, &volume_store->vs_region);
}

void prefetch_volume_pages(const struct volume_store *vs __maybe_unused,
			   unsigned int physical_page __maybe_unused,
			   unsigned int page_count __maybe_unused)
{
	/* Nothing to do in user mode */
}

int prepare_to_write_volume_page(const struct volume_store *volume_store
				 __maybe_unused,
				 unsigned int physical_page __maybe_unused,
				 struct volume_page *volume_page
				 __maybe_unused)
{
	/* Nothing to do in user mode */
	return UDS_SUCCESS;
}

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

void release_volume_page(struct volume_page *volume_page __maybe_unused)
{
	/* Nothing to do in user mode */
}

void swap_volume_pages(struct volume_page *volume_page1,
		       struct volume_page *volume_page2)
{
	struct volume_page temp = *volume_page1;
	*volume_page1 = *volume_page2;
	*volume_page2 = temp;
}

int sync_volume_store(const struct volume_store *volume_store)
{
	int result = sync_region_contents(volume_store->vs_region);
	if (result != UDS_SUCCESS) {
		return uds_log_error_strerror(result,
					      "cannot sync chapter to volume");
	}
	return UDS_SUCCESS;
}

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
