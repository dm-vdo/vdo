// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 *
 * If you add new statistics, be sure to update the following files:
 *
 * ../base/statistics.h
 * ../base/message-stats.c
 * ../base/pool-sysfs-stats.c
 * ./messageStatsReader.c
 * ../../../perl/Permabit/Statistics/Definitions.pm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "numeric.h"
#include "string-utils.h"

#include "math.h"
#include "statistics.h"
#include "status-codes.h"
#include "types.h"
#include "vdoStats.h"

#define MAX_STATS 239
#define MAX_STAT_LENGTH 80

int fieldCount = 0;
int maxLabelLength = 0;

char labels[MAX_STATS][MAX_STAT_LENGTH];
char values[MAX_STATS][MAX_STAT_LENGTH];


static int write_u8(char *label, u8 value)
{
	int count = sprintf(labels[fieldCount], "%s", label);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}

	maxLabelLength = max(maxLabelLength, (int) strlen(label));

	count = sprintf(values[fieldCount++], "%hhu", value);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int write_u64(char *label, u64 value)
{
	int count = sprintf(labels[fieldCount], "%s", label);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}

	maxLabelLength = max(maxLabelLength, (int) strlen(label));

	count = sprintf(values[fieldCount++], "%lu", value);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int write_string(char *label, char *value)
{
	int count = sprintf(labels[fieldCount], "%s", label);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}

	maxLabelLength = max(maxLabelLength, (int) strlen(label));

	count = sprintf(values[fieldCount++], "%s", value);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int write_block_count_t(char *label, block_count_t value)
{
	int count = sprintf(labels[fieldCount], "%s", label);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}

	maxLabelLength = max(maxLabelLength, (int) strlen(label));

	count = sprintf(values[fieldCount++], "%lu", value);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int write_u32(char *label, u32 value)
{
	int count = sprintf(labels[fieldCount], "%s", label);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}

	maxLabelLength = max(maxLabelLength, (int) strlen(label));

	count = sprintf(values[fieldCount++], "%u", value);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int write_double(char *label, double value)
{
	int count = sprintf(labels[fieldCount], "%s", label);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}

	maxLabelLength = max(maxLabelLength, (int) strlen(label));

	count = sprintf(values[fieldCount++], "%.2f", value);
	if (count < 0) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}


static int write_block_allocator_statistics(char *prefix,
					    struct block_allocator_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** The total number of slabs from which blocks may be allocated */
	if (asprintf(&joined, "%s slab count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->slab_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The total number of slabs from which blocks have ever been allocated */
	if (asprintf(&joined, "%s slabs opened", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->slabs_opened);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The number of times since loading that a slab has been re-opened */
	if (asprintf(&joined, "%s slabs reopened", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->slabs_reopened);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_commit_statistics(char *prefix,
				   struct commit_statistics *stats)
{
	int result = 0;
	char *joined = NULL;

	u64 batching = stats->started - stats->written;
	u64 writing = stats->written - stats->committed;

	if (asprintf(&joined, "%s batching", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, batching);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The total number of items on which processing has started */
	if (asprintf(&joined, "%s started", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->started);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s writing", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, writing);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The total number of items for which a write operation has been issued */
	if (asprintf(&joined, "%s written", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->written);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The total number of items for which a write operation has completed */
	if (asprintf(&joined, "%s committed", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->committed);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_recovery_journal_statistics(char *prefix,
					     struct recovery_journal_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** Number of times the on-disk journal was full */
	if (asprintf(&joined, "%s disk full count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->disk_full);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of times the recovery journal requested slab journal commits. */
	if (asprintf(&joined, "%s commits requested count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->slab_journal_commits_requested);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Write/Commit totals for individual journal entries */
	if (asprintf(&joined, "%s entries", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_commit_statistics(joined, &stats->entries);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Write/Commit totals for journal blocks */
	if (asprintf(&joined, "%s blocks", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_commit_statistics(joined, &stats->blocks);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_packer_statistics(char *prefix,
				   struct packer_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** Number of compressed data items written since startup */
	if (asprintf(&joined, "%s compressed fragments written", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->compressed_fragments_written);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of blocks containing compressed items written since startup */
	if (asprintf(&joined, "%s compressed blocks written", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->compressed_blocks_written);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of VIOs that are pending in the packer */
	if (asprintf(&joined, "%s compressed fragments in packer", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->compressed_fragments_in_packer);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_slab_journal_statistics(char *prefix,
					 struct slab_journal_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** Number of times the on-disk journal was full */
	if (asprintf(&joined, "%s disk full count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->disk_full_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of times an entry was added over the flush threshold */
	if (asprintf(&joined, "%s flush count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->flush_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of times an entry was added over the block threshold */
	if (asprintf(&joined, "%s blocked count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->blocked_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of times a tail block was written */
	if (asprintf(&joined, "%s blocks written", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->blocks_written);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of times we had to wait for the tail to write */
	if (asprintf(&joined, "%s tail busy count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->tail_busy_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_slab_summary_statistics(char *prefix,
					 struct slab_summary_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** Number of blocks written */
	if (asprintf(&joined, "%s blocks written", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->blocks_written);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_ref_counts_statistics(char *prefix,
				       struct ref_counts_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** Number of reference blocks written */
	if (asprintf(&joined, "%s blocks written", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->blocks_written);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_block_map_statistics(char *prefix,
				      struct block_map_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** number of dirty (resident) pages */
	if (asprintf(&joined, "%s dirty pages", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->dirty_pages);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of clean (resident) pages */
	if (asprintf(&joined, "%s clean pages", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->clean_pages);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of free pages */
	if (asprintf(&joined, "%s free pages", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->free_pages);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of pages in failed state */
	if (asprintf(&joined, "%s failed pages", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->failed_pages);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of pages incoming */
	if (asprintf(&joined, "%s incoming pages", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->incoming_pages);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of pages outgoing */
	if (asprintf(&joined, "%s outgoing pages", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->outgoing_pages);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** how many times free page not avail */
	if (asprintf(&joined, "%s cache pressure", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->cache_pressure);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of get_vdo_page() calls for read */
	if (asprintf(&joined, "%s read count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->read_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of get_vdo_page() calls for write */
	if (asprintf(&joined, "%s write count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->write_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of times pages failed to read */
	if (asprintf(&joined, "%s failed reads", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->failed_reads);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of times pages failed to write */
	if (asprintf(&joined, "%s failed writes", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->failed_writes);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of gets that are reclaimed */
	if (asprintf(&joined, "%s reclaimed", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->reclaimed);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of gets for outgoing pages */
	if (asprintf(&joined, "%s read outgoing", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->read_outgoing);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of gets that were already there */
	if (asprintf(&joined, "%s found in cache", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->found_in_cache);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of gets requiring discard */
	if (asprintf(&joined, "%s discard required", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->discard_required);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of gets enqueued for their page */
	if (asprintf(&joined, "%s wait for page", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->wait_for_page);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of gets that have to fetch */
	if (asprintf(&joined, "%s fetch required", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->fetch_required);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of page fetches */
	if (asprintf(&joined, "%s pages loaded", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->pages_loaded);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of page saves */
	if (asprintf(&joined, "%s pages saved", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->pages_saved);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** the number of flushes issued */
	if (asprintf(&joined, "%s flush count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->flush_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_hash_lock_statistics(char *prefix,
				      struct hash_lock_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** Number of times the UDS advice proved correct */
	if (asprintf(&joined, "%s dedupe advice valid", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->dedupe_advice_valid);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of times the UDS advice proved incorrect */
	if (asprintf(&joined, "%s dedupe advice stale", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->dedupe_advice_stale);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of writes with the same data as another in-flight write */
	if (asprintf(&joined, "%s concurrent data matches", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->concurrent_data_matches);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of writes whose hash collided with an in-flight write */
	if (asprintf(&joined, "%s concurrent hash collisions", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->concurrent_hash_collisions);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Current number of dedupe queries that are in flight */
	if (asprintf(&joined, "%s current dedupe queries", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->curr_dedupe_queries);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_error_statistics(char *prefix,
				  struct error_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** number of times VDO got an invalid dedupe advice PBN from UDS */
	if (asprintf(&joined, "%s invalid advice PBN count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->invalid_advice_pbn_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of times a VIO completed with a VDO_NO_SPACE error */
	if (asprintf(&joined, "%s no space error count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->no_space_error_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of times a VIO completed with a VDO_READ_ONLY error */
	if (asprintf(&joined, "%s read only error count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->read_only_error_count);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_bio_stats(char *prefix,
			   struct bio_stats *stats)
{
	int result = 0;
	char *joined = NULL;


	/** Number of REQ_OP_READ bios */
	if (asprintf(&joined, "%s read", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->read);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of REQ_OP_WRITE bios with data */
	if (asprintf(&joined, "%s write", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->write);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of bios tagged with REQ_PREFLUSH and containing no data */
	if (asprintf(&joined, "%s empty flush", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->empty_flush);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of REQ_OP_DISCARD bios */
	if (asprintf(&joined, "%s discard", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->discard);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of bios tagged with REQ_PREFLUSH */
	if (asprintf(&joined, "%s flush", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->flush);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of bios tagged with REQ_FUA */
	if (asprintf(&joined, "%s fua", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->fua);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_memory_usage(char *prefix,
			      struct memory_usage *stats)
{
	int result = 0;
	char *joined = NULL;


	/** Tracked bytes currently allocated. */
	if (asprintf(&joined, "%s bytes used", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->bytes_used);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Maximum tracked bytes allocated. */
	if (asprintf(&joined, "%s peak bytes used", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->peak_bytes_used);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_index_statistics(char *prefix,
				  struct index_statistics *stats)
{
	int result = 0;
	char *joined = NULL;


	/** Number of records stored in the index */
	if (asprintf(&joined, "%s entries indexed", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->entries_indexed);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of post calls that found an existing entry */
	if (asprintf(&joined, "%s posts found", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->posts_found);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of post calls that added a new entry */
	if (asprintf(&joined, "%s posts not found", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->posts_not_found);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of query calls that found an existing entry */
	if (asprintf(&joined, "%s queries found", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->queries_found);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of query calls that added a new entry */
	if (asprintf(&joined, "%s queries not found", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->queries_not_found);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of update calls that found an existing entry */
	if (asprintf(&joined, "%s updates found", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->updates_found);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of update calls that added a new entry */
	if (asprintf(&joined, "%s updates not found", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->updates_not_found);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of entries discarded */
	if (asprintf(&joined, "%s entries discarded", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->entries_discarded);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int write_vdo_statistics(char *prefix,
				struct vdo_statistics *stats)
{
	int result = 0;
	char *joined = NULL;

	u64 one_k_blocks = stats->physical_blocks * stats->block_size / 1024;
	u64 one_k_blocks_used = (stats->data_blocks_used + stats->overhead_blocks_used) * stats->block_size / 1024;
	u64 one_k_blocks_available = (stats->physical_blocks - stats->data_blocks_used - stats->overhead_blocks_used) * stats->block_size / 1024;
	u8 used_percent = (int) (100 * ((double) (stats->data_blocks_used + stats->overhead_blocks_used) / stats->physical_blocks) + 0.5);
	s32 savings = (stats->logical_blocks_used > 0) ? (int) (100 * (s64) (stats->logical_blocks_used - stats->data_blocks_used) / (u64) stats->logical_blocks_used) : 0;
	u8 saving_percent = savings;
	char five_twelve_byte_emulation[4] = "";
	sprintf(five_twelve_byte_emulation, "%s", (stats->logical_block_size == 512) ? "on" : "off");
	double write_amplification_ratio = (stats->bios_in.write > 0) ? roundf((double) (stats->bios_meta.write + stats->bios_out.write) / stats->bios_in.write) : 0.00;

	if (asprintf(&joined, "%s version", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->version);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of blocks used for data */
	if (asprintf(&joined, "%s data blocks used", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	if ((!stats->in_recovery_mode) && (strcmp("read-only", stats->mode))) {
		result = write_u64(joined, stats->data_blocks_used);
	} else {
		result = write_string(joined, "N/A");
	}
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of blocks used for VDO metadata */
	if (asprintf(&joined, "%s overhead blocks used", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	if (!stats->in_recovery_mode) {
		result = write_u64(joined, stats->overhead_blocks_used);
	} else {
		result = write_string(joined, "N/A");
	}
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of logical blocks that are currently mapped to physical blocks */
	if (asprintf(&joined, "%s logical blocks used", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	if (!stats->in_recovery_mode) {
		result = write_u64(joined, stats->logical_blocks_used);
	} else {
		result = write_string(joined, "N/A");
	}
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of physical blocks */
	if (asprintf(&joined, "%s physical blocks", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_block_count_t(joined, stats->physical_blocks);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** number of logical blocks */
	if (asprintf(&joined, "%s logical blocks", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_block_count_t(joined, stats->logical_blocks);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s 1K-blocks", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, one_k_blocks);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s 1K-blocks used", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	if ((!stats->in_recovery_mode) && (strcmp("read-only", stats->mode))) {
		result = write_u64(joined, one_k_blocks_used);
	} else {
		result = write_string(joined, "N/A");
	}
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s 1K-blocks available", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	if ((!stats->in_recovery_mode) && (strcmp("read-only", stats->mode))) {
		result = write_u64(joined, one_k_blocks_available);
	} else {
		result = write_string(joined, "N/A");
	}
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s used percent", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	if ((!stats->in_recovery_mode) && (strcmp("read-only", stats->mode))) {
		result = write_u8(joined, used_percent);
	} else {
		result = write_string(joined, "N/A");
	}
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s saving percent", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	if ((!stats->in_recovery_mode) && (strcmp("read-only", stats->mode)) && (savings >= 0)) {
		result = write_u8(joined, saving_percent);
	} else {
		result = write_string(joined, "N/A");
	}
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Size of the block map page cache, in bytes */
	if (asprintf(&joined, "%s block map cache size", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->block_map_cache_size);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The physical block size */
	if (asprintf(&joined, "%s block size", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->block_size);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of times the VDO has successfully recovered */
	if (asprintf(&joined, "%s completed recovery count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->complete_recoveries);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of times the VDO has recovered from read-only mode */
	if (asprintf(&joined, "%s read-only recovery count", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->read_only_recoveries);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** String describing the operating mode of the VDO */
	if (asprintf(&joined, "%s operating mode", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_string(joined, stats->mode);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** What percentage of recovery mode work has been completed */
	if (asprintf(&joined, "%s recovery progress (%%)", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	if (stats->in_recovery_mode) {
		result = write_u8(joined, stats->recovery_percentage);
	} else {
		result = write_string(joined, "N/A");
	}
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The statistics for the compressed block packer */
	if (asprintf(&joined, "%s", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_packer_statistics(joined, &stats->packer);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Counters for events in the block allocator */
	if (asprintf(&joined, "%s", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_block_allocator_statistics(joined, &stats->allocator);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Counters for events in the recovery journal */
	if (asprintf(&joined, "%s journal", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_recovery_journal_statistics(joined, &stats->journal);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The statistics for the slab journals */
	if (asprintf(&joined, "%s slab journal", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_slab_journal_statistics(joined, &stats->slab_journal);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The statistics for the slab summary */
	if (asprintf(&joined, "%s slab summary", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_slab_summary_statistics(joined, &stats->slab_summary);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The statistics for the reference counts */
	if (asprintf(&joined, "%s reference", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_ref_counts_statistics(joined, &stats->ref_counts);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The statistics for the block map */
	if (asprintf(&joined, "%s block map", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_block_map_statistics(joined, &stats->block_map);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The dedupe statistics from hash locks */
	if (asprintf(&joined, "%s", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_hash_lock_statistics(joined, &stats->hash_lock);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Counts of error conditions */
	if (asprintf(&joined, "%s", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_error_statistics(joined, &stats->errors);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The VDO instance */
	if (asprintf(&joined, "%s instance", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->instance);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s 512 byte emulation", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_string(joined, five_twelve_byte_emulation);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Current number of active VIOs */
	if (asprintf(&joined, "%s current VDO IO requests in progress", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->current_vios_in_progress);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Maximum number of active VIOs */
	if (asprintf(&joined, "%s maximum VDO IO requests in progress", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u32(joined, stats->max_vios);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of times the UDS index was too slow in responding */
	if (asprintf(&joined, "%s dedupe advice timeouts", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->dedupe_advice_timeouts);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Number of flush requests submitted to the storage device */
	if (asprintf(&joined, "%s flush out", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_u64(joined, stats->flush_out);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s write amplification ratio", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_double(joined, write_amplification_ratio);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Bios submitted into VDO from above */
	if (asprintf(&joined, "%s bios in", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_in);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s bios in partial", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_in_partial);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Bios submitted onward for user data */
	if (asprintf(&joined, "%s bios out", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_out);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Bios submitted onward for metadata */
	if (asprintf(&joined, "%s bios meta", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_meta);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s bios journal", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_journal);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s bios page cache", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_page_cache);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s bios out completed", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_out_completed);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s bios meta completed", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_meta_completed);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s bios journal completed", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_journal_completed);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s bios page cache completed", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_page_cache_completed);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s bios acknowledged", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_acknowledged);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	if (asprintf(&joined, "%s bios acknowledged partial", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_acknowledged_partial);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Current number of bios in progress */
	if (asprintf(&joined, "%s bios in progress", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_bio_stats(joined, &stats->bios_in_progress);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** Memory usage stats. */
	if (asprintf(&joined, "%s KVDO module", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_memory_usage(joined, &stats->memory_usage);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}

	/** The statistics for the UDS index */
	if (asprintf(&joined, "%s", prefix) == -1) {
		return VDO_UNEXPECTED_EOF;
	}
	result = write_index_statistics(joined, &stats->index);
	free(joined);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

int vdo_write_stats(struct vdo_statistics *stats)
{
	fieldCount = 0;
	maxLabelLength = 0;

	memset(labels, '\0', MAX_STATS * MAX_STAT_LENGTH);
	memset(values, '\0', MAX_STATS * MAX_STAT_LENGTH);

	int result = write_vdo_statistics(" ", stats);
	if (result != VDO_SUCCESS) {
		return result;
	}
	for (int i = 0; i < fieldCount; i++) {
		printf("%s%*s : %s\n",
		       labels[i],
		       maxLabelLength - (int) strlen(labels[i]), "",
		       values[i]);
	}
	return VDO_SUCCESS;
}
