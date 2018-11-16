"""
  Copyright (c) 2018 Red Hat, Inc.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA. 
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from .Field import *
from .StatStruct import *
from .VDOReleaseVersions import *

class BlockAllocatorStatistics(StatStruct):
  def __init__(self, name="BlockAllocatorStatistics", **kwargs):
    super(BlockAllocatorStatistics, self).__init__(name, [
      # The total number of slabs from which blocks may be allocated
      Uint64Field("slabCount"),
      # The total number of slabs from which blocks have ever been allocated
      Uint64Field("slabsOpened"),
      # The number of times since loading that a slab has been re-opened
      Uint64Field("slabsReopened"),
    ], procRoot="vdo", **kwargs)

# Counters for tracking the number of items written (blocks, requests, etc.)
# that keep track of totals at steps in the write pipeline. Three counters
# allow the number of buffered, in-memory items and the number of in-flight,
# unacknowledged writes to be derived, while still tracking totals for
# reporting purposes
class CommitStatistics(StatStruct):
  def __init__(self, name="CommitStatistics", **kwargs):
    super(CommitStatistics, self).__init__(name, [
      Uint64Field("batching", derived = "$started - $written"),
      # The total number of items on which processing has started
      Uint64Field("started"),
      Uint64Field("writing", derived = "$written - $committed"),
      # The total number of items for which a write operation has been issued
      Uint64Field("written"),
      # The total number of items for which a write operation has completed
      Uint64Field("committed"),
    ], procRoot="vdo", **kwargs)

# Counters for events in the recovery journal
class RecoveryJournalStatistics(StatStruct):
  def __init__(self, name="RecoveryJournalStatistics", **kwargs):
    super(RecoveryJournalStatistics, self).__init__(name, [
      # Number of times the on-disk journal was full
      Uint64Field("diskFull", label = "disk full count"),
      # Number of times the recovery journal requested slab journal commits.
      Uint64Field("slabJournalCommitsRequested", label = "commits requested count"),
      # Write/Commit totals for individual journal entries
      CommitStatistics("entries", labelPrefix = "entries"),
      # Write/Commit totals for journal blocks
      CommitStatistics("blocks", labelPrefix = "blocks"),
    ], labelPrefix="journal", procRoot="vdo", **kwargs)

# The statistics for the compressed block packer.
class PackerStatistics(StatStruct):
  def __init__(self, name="PackerStatistics", **kwargs):
    super(PackerStatistics, self).__init__(name, [
      # Number of compressed data items written since startup
      Uint64Field("compressedFragmentsWritten"),
      # Number of blocks containing compressed items written since startup
      Uint64Field("compressedBlocksWritten"),
      # Number of VIOs that are pending in the packer
      Uint64Field("compressedFragmentsInPacker"),
    ], procRoot="vdo", **kwargs)

# The statistics for the slab journals.
class SlabJournalStatistics(StatStruct):
  def __init__(self, name="SlabJournalStatistics", **kwargs):
    super(SlabJournalStatistics, self).__init__(name, [
      # Number of times the on-disk journal was full
      Uint64Field("diskFullCount"),
      # Number of times an entry was added over the flush threshold
      Uint64Field("flushCount"),
      # Number of times an entry was added over the block threshold
      Uint64Field("blockedCount"),
      # Number of times a tail block was written
      Uint64Field("blocksWritten"),
      # Number of times we had to wait for the tail to write
      Uint64Field("tailBusyCount"),
    ], labelPrefix="slab journal", procRoot="vdo", **kwargs)

# The statistics for the slab summary.
class SlabSummaryStatistics(StatStruct):
  def __init__(self, name="SlabSummaryStatistics", **kwargs):
    super(SlabSummaryStatistics, self).__init__(name, [
      # Number of blocks written
      Uint64Field("blocksWritten"),
    ], labelPrefix="slab summary", procRoot="vdo", **kwargs)

# The statistics for the reference counts.
class RefCountsStatistics(StatStruct):
  def __init__(self, name="RefCountsStatistics", **kwargs):
    super(RefCountsStatistics, self).__init__(name, [
      # Number of reference blocks written
      Uint64Field("blocksWritten"),
    ], labelPrefix="reference", procRoot="vdo", **kwargs)

# The statistics for the block map.
class BlockMapStatistics(StatStruct):
  def __init__(self, name="BlockMapStatistics", **kwargs):
    super(BlockMapStatistics, self).__init__(name, [
      # number of dirty (resident) pages
      Uint32Field("dirtyPages"),
      # number of clean (resident) pages
      Uint32Field("cleanPages"),
      # number of free pages
      Uint32Field("freePages"),
      # number of pages in failed state
      Uint32Field("failedPages"),
      # number of pages incoming
      Uint32Field("incomingPages"),
      # number of pages outgoing
      Uint32Field("outgoingPages"),
      # how many times free page not avail
      Uint32Field("cachePressure"),
      # number of getVDOPageAsync() for read
      Uint64Field("readCount"),
      # number or getVDOPageAsync() for write
      Uint64Field("writeCount"),
      # number of times pages failed to read
      Uint64Field("failedReads"),
      # number of times pages failed to write
      Uint64Field("failedWrites"),
      # number of gets that are reclaimed
      Uint64Field("reclaimed"),
      # number of gets for outgoing pages
      Uint64Field("readOutgoing"),
      # number of gets that were already there
      Uint64Field("foundInCache"),
      # number of gets requiring discard
      Uint64Field("discardRequired"),
      # number of gets enqueued for their page
      Uint64Field("waitForPage"),
      # number of gets that have to fetch
      Uint64Field("fetchRequired"),
      # number of page fetches
      Uint64Field("pagesLoaded"),
      # number of page saves
      Uint64Field("pagesSaved"),
      # the number of flushes issued
      Uint64Field("flushCount"),
    ], labelPrefix="block map", procRoot="vdo", **kwargs)

# The dedupe statistics from hash locks
class HashLockStatistics(StatStruct):
  def __init__(self, name="HashLockStatistics", **kwargs):
    super(HashLockStatistics, self).__init__(name, [
      # Number of times the UDS advice proved correct
      Uint64Field("dedupeAdviceValid"),
      # Number of times the UDS advice proved incorrect
      Uint64Field("dedupeAdviceStale"),
      # Number of writes with the same data as another in-flight write
      Uint64Field("concurrentDataMatches"),
      # Number of writes whose hash collided with an in-flight write
      Uint64Field("concurrentHashCollisions"),
    ], procRoot="vdo", **kwargs)

# Counts of error conditions in VDO.
class ErrorStatistics(StatStruct):
  def __init__(self, name="ErrorStatistics", **kwargs):
    super(ErrorStatistics, self).__init__(name, [
      # number of times VDO got an invalid dedupe advice PBN from UDS
      Uint64Field("invalidAdvicePBNCount"),
      # number of times a VIO completed with a VDO_NO_SPACE error
      Uint64Field("noSpaceErrorCount"),
      # number of times a VIO completed with a VDO_READ_ONLY error
      Uint64Field("readOnlyErrorCount"),
    ], procRoot="vdo", **kwargs)

# The statistics of the vdo service.
class VDOStatistics(StatStruct):
  def __init__(self, name="VDOStatistics", **kwargs):
    super(VDOStatistics, self).__init__(name, [
      Uint32Field("version"),
      Uint32Field("releaseVersion"),
      # Number of blocks used for data
      Uint64Field("dataBlocksUsed", available = "((not $inRecoveryMode) and ($mode != 'read-only'))"),
      # Number of blocks used for VDO metadata
      Uint64Field("overheadBlocksUsed", available = "not $inRecoveryMode"),
      # Number of logical blocks that are currently mapped to physical blocks
      Uint64Field("logicalBlocksUsed", available = "not $inRecoveryMode"),
      # number of physical blocks
      Uint64Field("physicalBlocks"),
      # number of logical blocks
      Uint64Field("logicalBlocks"),
      Uint64Field("oneKBlocks", label = "1K-blocks", derived = "$physicalBlocks * $blockSize // 1024"),
      Uint64Field("oneKBlocksUsed", label = "1K-blocks used", available = "not $inRecoveryMode", derived = "($dataBlocksUsed + $overheadBlocksUsed) * $blockSize // 1024"),
      Uint64Field("oneKBlocksAvailable", label = "1K-blocks available", available = "not $inRecoveryMode", derived = "($physicalBlocks - $dataBlocksUsed - $overheadBlocksUsed) * $blockSize // 1024"),
      Uint8Field("usedPercent", available = "((not $inRecoveryMode) and ($mode != 'read-only'))", derived = "int((100 * ($dataBlocksUsed + $overheadBlocksUsed) // $physicalBlocks) + 0.5)"),
      Uint8Field("savings", display = False, available = "not $inRecoveryMode", derived = "int(100 * ($logicalBlocksUsed - $dataBlocksUsed) // $logicalBlocksUsed) if ($logicalBlocksUsed > 0) else -1"),
      Uint8Field("savingPercent", available = "((not $inRecoveryMode) and ($mode != 'read-only'))", derived = "$savings if ($savings >= 0) else NotAvailable()"),
      # Size of the block map page cache, in bytes
      Uint64Field("blockMapCacheSize"),
      # String describing the active write policy of the VDO
      StringField("writePolicy", length = 15),
      # The physical block size
      Uint64Field("blockSize"),
      # Number of times the VDO has successfully recovered
      Uint64Field("completeRecoveries", label = "completed recovery count"),
      # Number of times the VDO has recovered from read-only mode
      Uint64Field("readOnlyRecoveries", label = "read-only recovery count"),
      # String describing the operating mode of the VDO
      StringField("mode", length = 15, label = "operating mode"),
      # Whether the VDO is in recovery mode
      BoolField("inRecoveryMode", display = False),
      # What percentage of recovery mode work has been completed
      Uint8Field("recoveryPercentage", label = "recovery progress (%)", available = "$inRecoveryMode"),
      # The statistics for the compressed block packer
      PackerStatistics("packer"),
      # Counters for events in the block allocator
      BlockAllocatorStatistics("allocator"),
      # Counters for events in the recovery journal
      RecoveryJournalStatistics("journal"),
      # The statistics for the slab journals
      SlabJournalStatistics("slabJournal"),
      # The statistics for the slab summary
      SlabSummaryStatistics("slabSummary"),
      # The statistics for the reference counts
      RefCountsStatistics("refCounts"),
      # The statistics for the block map
      BlockMapStatistics("blockMap"),
      # The dedupe statistics from hash locks
      HashLockStatistics("hashLock"),
      # Counts of error conditions
      ErrorStatistics("errors"),
    ], procFile="dedupe_stats", procRoot="vdo", **kwargs)

  statisticsVersion = 30

  def sample(self, device):
    sample = super(VDOStatistics, self).sample(device)
    if ((sample.getStat("version") != VDOStatistics.statisticsVersion) or (sample.getStat("releaseVersion") != CURRENT_RELEASE_VERSION_NUMBER)):
      raise Exception("VDOStatistics version mismatch")
    return sample

