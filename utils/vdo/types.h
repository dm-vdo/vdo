/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef VDO_TYPES_H
#define VDO_TYPES_H

#include <linux/compiler_attributes.h>
#include <linux/types.h>

#include "funnel-queue.h"

/* A size type in blocks. */
typedef u64 block_count_t;

/* The size of a block. */
typedef u16 block_size_t;

/* A counter for data_vios */
typedef u16 data_vio_count_t;

/* A height within a tree. */
typedef u8 height_t;

/* The logical block number as used by the consumer. */
typedef u64 logical_block_number_t;

/* The type of the nonce used to identify instances of VDO. */
typedef u64 nonce_t;

/* A size in pages. */
typedef u32 page_count_t;

/* A page number. */
typedef u32 page_number_t;

/*
 * The physical (well, less logical) block number at which the block is found on the underlying
 * device.
 */
typedef u64 physical_block_number_t;

/* A count of tree roots. */
typedef u8 root_count_t;

/* A number of sectors. */
typedef u8 sector_count_t;

/* A sequence number. */
typedef u64 sequence_number_t;

/* The offset of a block within a slab. */
typedef u32 slab_block_number;

/* A size type in slabs. */
typedef u16 slab_count_t;

/* A slot in a bin or block map page. */
typedef u16 slot_number_t;

/* typedef thread_count_t - A thread counter. */
typedef u8 thread_count_t;

/* typedef thread_id_t - A thread ID, vdo threads are numbered sequentially from 0. */
typedef u8 thread_id_t;

/* A zone counter */
typedef u8 zone_count_t;

/* The following enums are persisted on storage, so the values must be preserved. */

/* The current operating mode of the VDO. */
enum vdo_state {
	VDO_DIRTY = 0,
	VDO_NEW = 1,
	VDO_CLEAN = 2,
	VDO_READ_ONLY_MODE = 3,
	VDO_FORCE_REBUILD = 4,
	VDO_RECOVERING = 5,
	VDO_REPLAYING = 6, /* VDO_REPLAYING is never set anymore, but retained for upgrade */
	VDO_REBUILD_FOR_UPGRADE = 7,

	/* Keep VDO_STATE_COUNT at the bottom. */
	VDO_STATE_COUNT
};

/**
 * vdo_state_requires_read_only_rebuild() - Check whether a vdo_state indicates
 * that a read-only rebuild is required.
 * @state: The vdo_state to check.
 *
 * Return: true if the state indicates a rebuild is required
 */
static inline bool __must_check vdo_state_requires_read_only_rebuild(enum vdo_state state)
{
	return ((state == VDO_FORCE_REBUILD) || (state == VDO_REBUILD_FOR_UPGRADE));
}

/**
 * vdo_state_requires_recovery() - Check whether a vdo state indicates that recovery is needed.
 * @state: The state to check.
 *
 * Return: true if the state indicates a recovery is required
 */
static inline bool __must_check vdo_state_requires_recovery(enum vdo_state state)
{
	return ((state == VDO_DIRTY) || (state == VDO_REPLAYING) || (state == VDO_RECOVERING));
}

/*
 * The current operation on a physical block (from the point of view of the recovery journal, slab
 * journals, and reference counts.
 */
enum journal_operation {
	VDO_JOURNAL_DATA_REMAPPING = 0,
	VDO_JOURNAL_BLOCK_MAP_REMAPPING = 1,
} __packed;

/* Partition IDs encoded in the volume layout in the super block. */
enum partition_id {
	VDO_BLOCK_MAP_PARTITION = 0,
	VDO_SLAB_DEPOT_PARTITION = 1,
	VDO_RECOVERY_JOURNAL_PARTITION = 2,
	VDO_SLAB_SUMMARY_PARTITION = 3,
} __packed;

/* Metadata types for the vdo. */
enum vdo_metadata_type {
	VDO_METADATA_RECOVERY_JOURNAL = 1,
	VDO_METADATA_SLAB_JOURNAL = 2,
	VDO_METADATA_RECOVERY_JOURNAL_2 = 3,
} __packed;

/* A position in the block map where a block map entry is stored. */
struct block_map_slot {
	physical_block_number_t pbn;
	slot_number_t slot;
};

/*
 * Four bits of each five-byte block map entry contain a mapping state value used to distinguish
 * unmapped or discarded logical blocks (which are treated as mapped to the zero block) from entries
 * that have been mapped to a physical block, including the zero block.
 *
 * FIXME: these should maybe be defines.
 */
enum block_mapping_state {
	VDO_MAPPING_STATE_UNMAPPED = 0, /* Must be zero to be the default value */
	VDO_MAPPING_STATE_UNCOMPRESSED = 1, /* A normal (uncompressed) block */
	VDO_MAPPING_STATE_COMPRESSED_BASE = 2, /* Compressed in slot 0 */
	VDO_MAPPING_STATE_COMPRESSED_MAX = 15, /* Compressed in slot 13 */
};

enum {
	VDO_MAX_COMPRESSION_SLOTS =
		(VDO_MAPPING_STATE_COMPRESSED_MAX - VDO_MAPPING_STATE_COMPRESSED_BASE + 1),
};


struct data_location {
	physical_block_number_t pbn;
	enum block_mapping_state state;
};

/* The configuration of a single slab derived from the configured block size and slab size. */
struct slab_config {
	/* total number of blocks in the slab */
	block_count_t slab_blocks;
	/* number of blocks available for data */
	block_count_t data_blocks;
	/* number of blocks for reference counts */
	block_count_t reference_count_blocks;
	/* number of blocks for the slab journal */
	block_count_t slab_journal_blocks;
	/*
	 * Number of blocks after which the slab journal starts pushing out a reference_block for
	 * each new entry it receives.
	 */
	block_count_t slab_journal_flushing_threshold;
	/*
	 * Number of blocks after which the slab journal pushes out all reference_blocks and makes
	 * all vios wait.
	 */
	block_count_t slab_journal_blocking_threshold;
	/* Number of blocks after which the slab must be scrubbed before coming online. */
	block_count_t slab_journal_scrubbing_threshold;
} __packed;

struct vdo_config;

#endif /* VDO_TYPES_H */
