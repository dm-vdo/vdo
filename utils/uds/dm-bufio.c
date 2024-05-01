// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include <linux/dm-bufio.h>

#include <linux/blkdev.h>
#include <linux/err.h>

#include "fileUtils.h"
#include "logger.h"
#include "memory-alloc.h"
#include "thread-utils.h"

/*
 * This fake client does not actually do any type of sophisticated buffering.
 * Instead, it hands out buffers from a list of saved dm_buffer objects,
 * creating new ones when necessary. When a buffer is marked dirty, the client
 * writes its data immediately so that it can return the buffer to circulation
 * and not have to track unsaved buffers.
 */

struct dm_buffer;

struct dm_bufio_client {
	int status;
	struct block_device *bdev;
	off_t start_offset;
	size_t bytes_per_page;

	struct mutex buffer_mutex;
	struct dm_buffer *buffer_list;
};

struct dm_buffer {
	struct dm_bufio_client *client;
	struct dm_buffer *next;
	sector_t offset;
	u8 *data;
};

struct dm_bufio_client *
dm_bufio_client_create(struct block_device *bdev,
		       unsigned block_size,
		       unsigned reserved_buffers __always_unused,
		       unsigned aux_size __always_unused,
		       void (*alloc_callback)(struct dm_buffer *)
		       __always_unused,
		       void (*write_callback)(struct dm_buffer *)
		       __always_unused,
		       unsigned int flags __always_unused)
{
	int result;
	struct dm_bufio_client *client;

	result = vdo_allocate(1, struct dm_bufio_client, __func__, &client);
	if (result != VDO_SUCCESS)
		return ERR_PTR(-ENOMEM);


	result = uds_init_mutex(&client->buffer_mutex);
	if (result != UDS_SUCCESS) {
		dm_bufio_client_destroy(client);
		return ERR_PTR(-result);
	}

	client->bytes_per_page = block_size;
	client->bdev = bdev;
	return client;
}

void dm_bufio_client_destroy(struct dm_bufio_client *client)
{
	struct dm_buffer *buffer;

	while (client->buffer_list != NULL) {
		buffer = client->buffer_list;
		client->buffer_list = buffer->next;
		vdo_free(buffer->data);
		vdo_free(buffer);
	}

	uds_destroy_mutex(&client->buffer_mutex);
	vdo_free(client);
}

void dm_bufio_set_sector_offset(struct dm_bufio_client *client, sector_t start)
{
	client->start_offset = start * SECTOR_SIZE;
}

void *dm_bufio_new(struct dm_bufio_client *client,
		   sector_t block,
		   struct dm_buffer **buffer_ptr)
{
	int result;
	struct dm_buffer *buffer = NULL;
	off_t block_offset = block * client->bytes_per_page;

	uds_lock_mutex(&client->buffer_mutex);
	if (client->buffer_list != NULL) {
		buffer = client->buffer_list;
		client->buffer_list = buffer->next;
	}
	uds_unlock_mutex(&client->buffer_mutex);

	if (buffer == NULL) {
		result = vdo_allocate(1, struct dm_buffer, __func__, &buffer);
		if (result != VDO_SUCCESS)
			return ERR_PTR(-ENOMEM);

		result = vdo_allocate(client->bytes_per_page,
				      u8,
				      __func__,
				      &buffer->data);
		if (result != VDO_SUCCESS) {
			vdo_free(buffer);
			return ERR_PTR(-ENOMEM);
		}

		buffer->client = client;
	}

	buffer->offset = client->start_offset + block_offset;
	*buffer_ptr = buffer;
	return buffer->data;
}

/* This gets a new buffer to read data into. */
void *dm_bufio_read(struct dm_bufio_client *client,
		    sector_t block,
		    struct dm_buffer **buffer_ptr)
{
	int result;
	size_t read_length = 0;
	struct dm_buffer *buffer;
	u8 *data;

	data = dm_bufio_new(client, block, &buffer);
	if (IS_ERR(data)) {
		vdo_log_error_strerror(-PTR_ERR(data),
				       "error reading physical page %lu",
				       block);
		return data;
	}

	result = read_data_at_offset(client->bdev->fd,
				     buffer->offset,
				     buffer->data,
				     client->bytes_per_page,
				     &read_length);
	if (result != UDS_SUCCESS) {
		dm_bufio_release(buffer);
		vdo_log_warning_strerror(result,
					 "error reading physical page %lu",
					 block);
		return ERR_PTR(-EIO);
	}

	if (read_length < client->bytes_per_page)
		memset(&buffer->data[read_length],
		       0,
		       client->bytes_per_page - read_length);

	*buffer_ptr = buffer;
	return buffer->data;
}

void dm_bufio_prefetch(struct dm_bufio_client *client __always_unused,
		       sector_t block __always_unused,
		       unsigned block_count __always_unused)
{
	/* Prefetching is meaningless when dealing with files. */
}

void dm_bufio_release(struct dm_buffer *buffer)
{
	struct dm_bufio_client *client = buffer->client;

	uds_lock_mutex(&client->buffer_mutex);
	buffer->next = client->buffer_list;
	client->buffer_list = buffer;
	uds_unlock_mutex(&client->buffer_mutex);
}

/*
 * This moves the buffer from its current location to a new one without
 * changing the buffer contents. dm_bufio_mark_buffer_dirty() is required to
 * write the buffer contents to the new location.
 */
void dm_bufio_release_move(struct dm_buffer *buffer, sector_t new_block)
{
	struct dm_bufio_client *client = buffer->client;
	off_t block_offset = new_block * client->bytes_per_page;

	buffer->offset = client->start_offset + block_offset;
}

/* Write the buffer immediately rather than have to track dirty buffers. */
void dm_bufio_mark_buffer_dirty(struct dm_buffer *buffer)
{
	int result;
	struct dm_bufio_client *client = buffer->client;

	result = write_buffer_at_offset(client->bdev->fd,
					buffer->offset,
					buffer->data,
					client->bytes_per_page);
	if (client->status == UDS_SUCCESS)
		client->status = result;
}

/* Since we already wrote all the dirty buffers, just sync the file. */
int dm_bufio_write_dirty_buffers(struct dm_bufio_client *client)
{
	if (client->status != UDS_SUCCESS)
		return -client->status;

	return -logging_fsync(client->bdev->fd, "cannot sync file contents");
}

void *dm_bufio_get_block_data(struct dm_buffer *buffer)
{
	return buffer->data;
}
