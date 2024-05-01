// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include "string-utils.h"

#include "statistics.h"
#include "status-codes.h"
#include "vdoStats.h"

static int skip_string(char **buf, char *skip)
{
        char *tmp = NULL;
        tmp = strstr(*buf, skip);
        if (tmp == NULL) {
                return VDO_UNEXPECTED_EOF;
        }
        *buf = tmp + strlen(skip);
        return VDO_SUCCESS;
}

static int read_u64(char **buf,
		    u64 *value)
{
	int count = sscanf(*buf, "%lu, ", value);
	if (count != 1) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int read_u32(char **buf,
		    u32 *value)
{
	int count = sscanf(*buf, "%u, ", value);
	if (count != 1) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int read_block_count_t(char **buf,
			      block_count_t *value)
{
	int count = sscanf(*buf, "%lu, ", value);
	if (count != 1) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int read_string(char **buf,
		       char *value)
{
	int count = sscanf(*buf, "%[^,], ", value);
	if (count != 1) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int read_bool(char **buf,
		     bool *value)
{
	int temp;
	int count = sscanf(*buf, "%d, ", &temp);
	*value = (bool)temp;
	if (count != 1) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int read_u8(char **buf,
		   u8 *value)
{
	int count = sscanf(*buf, "%hhu, ", value);
	if (count != 1) {
		return VDO_UNEXPECTED_EOF;
	}
	return VDO_SUCCESS;
}

static int read_block_allocator_statistics(char **buf,
					   struct block_allocator_statistics *stats)
{
	int result = 0;

	/** The total number of slabs from which blocks may be allocated */
	result = skip_string(buf, "slabCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->slab_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The total number of slabs from which blocks have ever been allocated */
	result = skip_string(buf, "slabsOpened : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->slabs_opened);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The number of times since loading that a slab has been re-opened */
	result = skip_string(buf, "slabsReopened : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->slabs_reopened);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_commit_statistics(char **buf,
				  struct commit_statistics *stats)
{
	int result = 0;

	/** The total number of items on which processing has started */
	result = skip_string(buf, "started : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->started);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The total number of items for which a write operation has been issued */
	result = skip_string(buf, "written : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->written);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The total number of items for which a write operation has completed */
	result = skip_string(buf, "committed : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->committed);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_recovery_journal_statistics(char **buf,
					    struct recovery_journal_statistics *stats)
{
	int result = 0;

	/** Number of times the on-disk journal was full */
	result = skip_string(buf, "diskFull : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->disk_full);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of times the recovery journal requested slab journal commits. */
	result = skip_string(buf, "slabJournalCommitsRequested : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->slab_journal_commits_requested);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Write/Commit totals for individual journal entries */
	result = skip_string(buf, "entries : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_commit_statistics(buf,
					&stats->entries);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Write/Commit totals for journal blocks */
	result = skip_string(buf, "blocks : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_commit_statistics(buf,
					&stats->blocks);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_packer_statistics(char **buf,
				  struct packer_statistics *stats)
{
	int result = 0;

	/** Number of compressed data items written since startup */
	result = skip_string(buf, "compressedFragmentsWritten : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->compressed_fragments_written);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of blocks containing compressed items written since startup */
	result = skip_string(buf, "compressedBlocksWritten : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->compressed_blocks_written);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of VIOs that are pending in the packer */
	result = skip_string(buf, "compressedFragmentsInPacker : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->compressed_fragments_in_packer);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_slab_journal_statistics(char **buf,
					struct slab_journal_statistics *stats)
{
	int result = 0;

	/** Number of times the on-disk journal was full */
	result = skip_string(buf, "diskFullCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->disk_full_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of times an entry was added over the flush threshold */
	result = skip_string(buf, "flushCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->flush_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of times an entry was added over the block threshold */
	result = skip_string(buf, "blockedCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->blocked_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of times a tail block was written */
	result = skip_string(buf, "blocksWritten : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->blocks_written);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of times we had to wait for the tail to write */
	result = skip_string(buf, "tailBusyCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->tail_busy_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_slab_summary_statistics(char **buf,
					struct slab_summary_statistics *stats)
{
	int result = 0;

	/** Number of blocks written */
	result = skip_string(buf, "blocksWritten : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->blocks_written);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_ref_counts_statistics(char **buf,
				      struct ref_counts_statistics *stats)
{
	int result = 0;

	/** Number of reference blocks written */
	result = skip_string(buf, "blocksWritten : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->blocks_written);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_block_map_statistics(char **buf,
				     struct block_map_statistics *stats)
{
	int result = 0;

	/** number of dirty (resident) pages */
	result = skip_string(buf, "dirtyPages : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->dirty_pages);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of clean (resident) pages */
	result = skip_string(buf, "cleanPages : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->clean_pages);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of free pages */
	result = skip_string(buf, "freePages : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->free_pages);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of pages in failed state */
	result = skip_string(buf, "failedPages : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->failed_pages);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of pages incoming */
	result = skip_string(buf, "incomingPages : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->incoming_pages);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of pages outgoing */
	result = skip_string(buf, "outgoingPages : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->outgoing_pages);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** how many times free page not avail */
	result = skip_string(buf, "cachePressure : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->cache_pressure);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of get_vdo_page() calls for read */
	result = skip_string(buf, "readCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->read_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of get_vdo_page() calls for write */
	result = skip_string(buf, "writeCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->write_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of times pages failed to read */
	result = skip_string(buf, "failedReads : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->failed_reads);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of times pages failed to write */
	result = skip_string(buf, "failedWrites : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->failed_writes);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of gets that are reclaimed */
	result = skip_string(buf, "reclaimed : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->reclaimed);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of gets for outgoing pages */
	result = skip_string(buf, "readOutgoing : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->read_outgoing);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of gets that were already there */
	result = skip_string(buf, "foundInCache : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->found_in_cache);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of gets requiring discard */
	result = skip_string(buf, "discardRequired : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->discard_required);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of gets enqueued for their page */
	result = skip_string(buf, "waitForPage : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->wait_for_page);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of gets that have to fetch */
	result = skip_string(buf, "fetchRequired : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->fetch_required);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of page fetches */
	result = skip_string(buf, "pagesLoaded : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->pages_loaded);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of page saves */
	result = skip_string(buf, "pagesSaved : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->pages_saved);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** the number of flushes issued */
	result = skip_string(buf, "flushCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->flush_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_hash_lock_statistics(char **buf,
				     struct hash_lock_statistics *stats)
{
	int result = 0;

	/** Number of times the UDS advice proved correct */
	result = skip_string(buf, "dedupeAdviceValid : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->dedupe_advice_valid);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of times the UDS advice proved incorrect */
	result = skip_string(buf, "dedupeAdviceStale : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->dedupe_advice_stale);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of writes with the same data as another in-flight write */
	result = skip_string(buf, "concurrentDataMatches : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->concurrent_data_matches);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of writes whose hash collided with an in-flight write */
	result = skip_string(buf, "concurrentHashCollisions : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->concurrent_hash_collisions);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Current number of dedupe queries that are in flight */
	result = skip_string(buf, "currDedupeQueries : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->curr_dedupe_queries);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_error_statistics(char **buf,
				 struct error_statistics *stats)
{
	int result = 0;

	/** number of times VDO got an invalid dedupe advice PBN from UDS */
	result = skip_string(buf, "invalidAdvicePBNCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->invalid_advice_pbn_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of times a VIO completed with a VDO_NO_SPACE error */
	result = skip_string(buf, "noSpaceErrorCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->no_space_error_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of times a VIO completed with a VDO_READ_ONLY error */
	result = skip_string(buf, "readOnlyErrorCount : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->read_only_error_count);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_bio_stats(char **buf,
			  struct bio_stats *stats)
{
	int result = 0;

	/** Number of REQ_OP_READ bios */
	result = skip_string(buf, "read : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->read);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of REQ_OP_WRITE bios with data */
	result = skip_string(buf, "write : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->write);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of bios tagged with REQ_PREFLUSH and containing no data */
	result = skip_string(buf, "emptyFlush : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->empty_flush);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of REQ_OP_DISCARD bios */
	result = skip_string(buf, "discard : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->discard);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of bios tagged with REQ_PREFLUSH */
	result = skip_string(buf, "flush : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->flush);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of bios tagged with REQ_FUA */
	result = skip_string(buf, "fua : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->fua);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_memory_usage(char **buf,
			     struct memory_usage *stats)
{
	int result = 0;

	/** Tracked bytes currently allocated. */
	result = skip_string(buf, "bytesUsed : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->bytes_used);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Maximum tracked bytes allocated. */
	result = skip_string(buf, "peakBytesUsed : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->peak_bytes_used);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_index_statistics(char **buf,
				 struct index_statistics *stats)
{
	int result = 0;

	/** Number of records stored in the index */
	result = skip_string(buf, "entriesIndexed : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->entries_indexed);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of post calls that found an existing entry */
	result = skip_string(buf, "postsFound : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->posts_found);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of post calls that added a new entry */
	result = skip_string(buf, "postsNotFound : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->posts_not_found);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of query calls that found an existing entry */
	result = skip_string(buf, "queriesFound : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->queries_found);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of query calls that added a new entry */
	result = skip_string(buf, "queriesNotFound : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->queries_not_found);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of update calls that found an existing entry */
	result = skip_string(buf, "updatesFound : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->updates_found);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of update calls that added a new entry */
	result = skip_string(buf, "updatesNotFound : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->updates_not_found);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of entries discarded */
	result = skip_string(buf, "entriesDiscarded : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->entries_discarded);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

static int read_vdo_statistics(char **buf,
			       struct vdo_statistics *stats)
{
	int result = 0;

	result = skip_string(buf, "version : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->version);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of blocks used for data */
	result = skip_string(buf, "dataBlocksUsed : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->data_blocks_used);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of blocks used for VDO metadata */
	result = skip_string(buf, "overheadBlocksUsed : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->overhead_blocks_used);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of logical blocks that are currently mapped to physical blocks */
	result = skip_string(buf, "logicalBlocksUsed : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->logical_blocks_used);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of physical blocks */
	result = skip_string(buf, "physicalBlocks : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_block_count_t(buf,
				    &stats->physical_blocks);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** number of logical blocks */
	result = skip_string(buf, "logicalBlocks : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_block_count_t(buf,
				    &stats->logical_blocks);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Size of the block map page cache, in bytes */
	result = skip_string(buf, "blockMapCacheSize : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->block_map_cache_size);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The physical block size */
	result = skip_string(buf, "blockSize : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->block_size);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of times the VDO has successfully recovered */
	result = skip_string(buf, "completeRecoveries : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->complete_recoveries);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of times the VDO has recovered from read-only mode */
	result = skip_string(buf, "readOnlyRecoveries : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->read_only_recoveries);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** String describing the operating mode of the VDO */
	result = skip_string(buf, "mode : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_string(buf,
			     stats->mode);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Whether the VDO is in recovery mode */
	result = skip_string(buf, "inRecoveryMode : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bool(buf,
			   &stats->in_recovery_mode);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** What percentage of recovery mode work has been completed */
	result = skip_string(buf, "recoveryPercentage : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u8(buf,
			 &stats->recovery_percentage);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The statistics for the compressed block packer */
	result = skip_string(buf, "packer : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_packer_statistics(buf,
					&stats->packer);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Counters for events in the block allocator */
	result = skip_string(buf, "allocator : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_block_allocator_statistics(buf,
						 &stats->allocator);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Counters for events in the recovery journal */
	result = skip_string(buf, "journal : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_recovery_journal_statistics(buf,
						  &stats->journal);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The statistics for the slab journals */
	result = skip_string(buf, "slabJournal : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_slab_journal_statistics(buf,
					      &stats->slab_journal);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The statistics for the slab summary */
	result = skip_string(buf, "slabSummary : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_slab_summary_statistics(buf,
					      &stats->slab_summary);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The statistics for the reference counts */
	result = skip_string(buf, "refCounts : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_ref_counts_statistics(buf,
					    &stats->ref_counts);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The statistics for the block map */
	result = skip_string(buf, "blockMap : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_block_map_statistics(buf,
					   &stats->block_map);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The dedupe statistics from hash locks */
	result = skip_string(buf, "hashLock : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_hash_lock_statistics(buf,
					   &stats->hash_lock);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Counts of error conditions */
	result = skip_string(buf, "errors : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_error_statistics(buf,
				       &stats->errors);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The VDO instance */
	result = skip_string(buf, "instance : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->instance);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Current number of active VIOs */
	result = skip_string(buf, "currentVIOsInProgress : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->current_vios_in_progress);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Maximum number of active VIOs */
	result = skip_string(buf, "maxVIOs : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u32(buf,
			  &stats->max_vios);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of times the UDS index was too slow in responding */
	result = skip_string(buf, "dedupeAdviceTimeouts : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->dedupe_advice_timeouts);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Number of flush requests submitted to the storage device */
	result = skip_string(buf, "flushOut : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->flush_out);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Logical block size */
	result = skip_string(buf, "logicalBlockSize : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_u64(buf,
			  &stats->logical_block_size);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Bios submitted into VDO from above */
	result = skip_string(buf, "biosIn : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_in);
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = skip_string(buf, "biosInPartial : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_in_partial);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Bios submitted onward for user data */
	result = skip_string(buf, "biosOut : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_out);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Bios submitted onward for metadata */
	result = skip_string(buf, "biosMeta : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_meta);
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = skip_string(buf, "biosJournal : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_journal);
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = skip_string(buf, "biosPageCache : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_page_cache);
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = skip_string(buf, "biosOutCompleted : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_out_completed);
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = skip_string(buf, "biosMetaCompleted : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_meta_completed);
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = skip_string(buf, "biosJournalCompleted : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_journal_completed);
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = skip_string(buf, "biosPageCacheCompleted : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_page_cache_completed);
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = skip_string(buf, "biosAcknowledged : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_acknowledged);
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = skip_string(buf, "biosAcknowledgedPartial : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_acknowledged_partial);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Current number of bios in progress */
	result = skip_string(buf, "biosInProgress : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_bio_stats(buf,
				&stats->bios_in_progress);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** Memory usage stats. */
	result = skip_string(buf, "memoryUsage : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_memory_usage(buf,
				   &stats->memory_usage);
	if (result != VDO_SUCCESS) {
		return result;
	}
	/** The statistics for the UDS index */
	result = skip_string(buf, "index : ");
	if (result != VDO_SUCCESS) {
		return result;
	}
	result = read_index_statistics(buf,
				       &stats->index);
	if (result != VDO_SUCCESS) {
		return result;
	}
	return VDO_SUCCESS;
}

int read_vdo_stats(char *buf,
		   struct vdo_statistics *stats)
{
	return(read_vdo_statistics(&buf, stats));
}
