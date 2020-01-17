/*
 * Copyright (c) 2020 Red Hat, Inc.
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
 * $Id: //eng/linux-vdo/src/c++/vdo/base/packerInternals.h#11 $
 */

#ifndef PACKER_INTERNALS_H
#define PACKER_INTERNALS_H

#include "packer.h"

#include "atomic.h"

#include "adminState.h"
#include "compressedBlock.h"
#include "header.h"
#include "types.h"
#include "waitQueue.h"

/**
 * Each input_bin holds an incomplete batch of DataVIOs that only partially fill
 * a compressed block. The InputBins are kept in a ring sorted by the amount of
 * unused space so the first bin with enough space to hold a newly-compressed
 * data_vio can easily be found. When the bin fills up or is flushed, the
 * incoming DataVIOs are moved to the packer's batchedDataVIOs queue, from
 * which they will eventually be routed to an idle output_bin.
 *
 * There is one special input bin which is used to hold DataVIOs which have
 * been canceled and removed from their input bin by the packer. These DataVIOs
 * need to wait for the canceller to rendezvous with them (VDO-2809) and so
 * they sit in this special bin.
 **/
struct input_bin {
  /** List links for packer.sortedBins */
  RingNode         ring;
  /** The number of items in the bin */
  SlotNumber       slotsUsed;
  /** The number of compressed block bytes remaining in the current batch */
  size_t           freeSpace;
  /** The current partial batch of DataVIOs, waiting for more */
  struct data_vio *incoming[];
};

/**
 * Each output_bin allows a single compressed block to be packed and written.
 * When it is not idle, it holds a batch of DataVIOs that have been packed
 * into the compressed block, written asynchronously, and are waiting for the
 * write to complete.
 **/
struct output_bin {
  /** List links for packer.outputBins */
  RingNode                 ring;
  /** The storage for encoding the compressed block representation */
  struct compressed_block *block;
  /** The struct allocating_vio wrapping the compressed block for writing */
  struct allocating_vio   *writer;
  /** The number of compression slots used in the compressed block */
  SlotNumber               slotsUsed;
  /** The DataVIOs packed into the block, waiting for the write to complete */
  struct wait_queue        outgoing;
};

/**
 * A counted array holding a batch of DataVIOs that should be packed into an
 * output bin.
 **/
struct output_batch {
  size_t           slotsUsed;
  struct data_vio *slots[MAX_COMPRESSION_SLOTS];
};

struct packer {
  /** The ID of the packer's callback thread */
  ThreadID                    threadID;
  /** The selector for determining which physical zone to allocate from */
  struct allocation_selector *selector;
  /** The number of input bins */
  BlockCount                  size;
  /** The block size minus header size */
  size_t                      binDataSize;
  /** The number of compression slots */
  size_t                      maxSlots;
  /** A ring of all InputBins, kept sorted by freeSpace */
  RingNode                    inputBins;
  /** A ring of all OutputBins */
  RingNode                    outputBins;
  /**
   * A bin to hold DataVIOs which were canceled out of the packer and are
   * waiting to rendezvous with the canceling data_vio.
   **/
  struct input_bin           *canceledBin;

  /** The current flush generation */
  SequenceNumber              flushGeneration;

  /** The administrative state of the packer */
  struct admin_state          state;
  /** True when writing batched DataVIOs */
  bool                        writingBatches;

  // Atomic counters corresponding to the fields of PackerStatistics:

  /** Number of compressed data items written since startup */
  Atomic64                    fragmentsWritten;
  /** Number of blocks containing compressed items written since startup */
  Atomic64                    blocksWritten;
  /** Number of DataVIOs that are pending in the packer */
  Atomic64                    fragmentsPending;

  /** Queue of batched DataVIOs waiting to be packed */
  struct wait_queue           batchedDataVIOs;

  /** The total number of output bins allocated */
  size_t                      outputBinCount;
  /** The number of idle output bins on the stack */
  size_t                      idleOutputBinCount;
  /** The stack of idle output bins (0 = bottom) */
  struct output_bin          *idleOutputBins[];
};

/**
 * This returns the first bin in the freeSpace-sorted list.
 **/
struct input_bin *getFullestBin(const struct packer *packer);

/**
 * This returns the next bin in the freeSpace-sorted list.
 **/
struct input_bin *nextBin(const struct packer *packer, struct input_bin *bin);

/**
 * Change the maxiumum number of compression slots the packer will use. The new
 * number of slots must be less than or equal to MAX_COMPRESSION_SLOTS. Bins
 * which already have fragments will not be resized until they are next written
 * out.
 *
 * @param packer  The packer
 * @param slots   The new number of slots
 **/
void resetSlotCount(struct packer *packer, CompressedFragmentCount slots);

/**
 * Remove a data_vio from the packer. This method is exposed for testing.
 *
 * @param dataVIO  The data_vio to remove
 **/
void removeFromPacker(struct data_vio *dataVIO);

#endif /* PACKER_INTERNALS_H */
