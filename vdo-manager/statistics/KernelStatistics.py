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

class BioStats(StatStruct):
  def __init__(self, name="BioStats", **kwargs):
    super(BioStats, self).__init__(name, [
      # Number of not REQ_WRITE bios
      Uint64Field("read"),
      # Number of REQ_WRITE bios
      Uint64Field("write"),
      # Number of REQ_DISCARD bios
      Uint64Field("discard"),
      # Number of REQ_FLUSH bios
      Uint64Field("flush"),
      # Number of REQ_FUA bios
      Uint64Field("fua"),
    ], procRoot="vdo", **kwargs)

class MemoryUsage(StatStruct):
  def __init__(self, name="MemoryUsage", **kwargs):
    super(MemoryUsage, self).__init__(name, [
      # Tracked bytes currently allocated.
      Uint64Field("bytesUsed"),
      # Maximum tracked bytes allocated.
      Uint64Field("peakBytesUsed"),
      # Bio structures currently allocated (size not tracked).
      Uint64Field("biosUsed"),
      # Maximum number of bios allocated.
      Uint64Field("peakBioCount"),
    ], procRoot="vdo", **kwargs)

# UDS index statistics
class IndexStatistics(StatStruct):
  def __init__(self, name="IndexStatistics", **kwargs):
    super(IndexStatistics, self).__init__(name, [
      # Number of chunk names stored in the index
      Uint64Field("entriesIndexed"),
      # Number of post calls that found an existing entry
      Uint64Field("postsFound"),
      # Number of post calls that added a new entry
      Uint64Field("postsNotFound"),
      # Number of query calls that found an existing entry
      Uint64Field("queriesFound"),
      # Number of query calls that added a new entry
      Uint64Field("queriesNotFound"),
      # Number of update calls that found an existing entry
      Uint64Field("updatesFound"),
      # Number of update calls that added a new entry
      Uint64Field("updatesNotFound"),
      # Current number of dedupe queries that are in flight
      Uint32Field("currDedupeQueries", label = "current dedupe queries"),
      # Maximum number of dedupe queries that have been in flight
      Uint32Field("maxDedupeQueries", label = "maximum dedupe queries"),
    ], procRoot="vdo", **kwargs)

class KernelStatistics(StatStruct):
  def __init__(self, name="KernelStatistics", **kwargs):
    super(KernelStatistics, self).__init__(name, [
      Uint32Field("version", display = False),
      Uint32Field("releaseVersion", display = False),
      # The VDO instance
      Uint32Field("instance"),
      StringField("fiveTwelveByteEmulation", label = "512 byte emulation", derived = "'on' if ($logicalBlockSize == 512) else 'off'"),
      # Current number of active VIOs
      Uint32Field("currentVIOsInProgress", label = "current VDO IO requests in progress"),
      # Maximum number of active VIOs
      Uint32Field("maxVIOs", label = "maximum VDO IO requests in progress"),
      # Number of times the UDS index was too slow in responding
      Uint64Field("dedupeAdviceTimeouts"),
      # Number of flush requests submitted to the storage device
      Uint64Field("flushOut"),
      # Logical block size
      Uint64Field("logicalBlockSize", display = False),
      FloatField("writeAmplificationRatio", derived = "round(($biosMeta[\"write\"] + $biosOut[\"write\"]) // float($biosIn[\"write\"]), 2) if $biosIn[\"write\"] > 0 else 0.00"),
      # Bios submitted into VDO from above
      BioStats("biosIn", labelPrefix = "bios in"),
      BioStats("biosInPartial", labelPrefix = "bios in partial"),
      # Bios submitted onward for user data
      BioStats("biosOut", labelPrefix = "bios out"),
      # Bios submitted onward for metadata
      BioStats("biosMeta", labelPrefix = "bios meta"),
      BioStats("biosJournal", labelPrefix = "bios journal"),
      BioStats("biosPageCache", labelPrefix = "bios page cache"),
      BioStats("biosOutCompleted", labelPrefix = "bios out completed"),
      BioStats("biosMetaCompleted", labelPrefix = "bios meta completed"),
      BioStats("biosJournalCompleted", labelPrefix = "bios journal completed"),
      BioStats("biosPageCacheCompleted", labelPrefix = "bios page cache completed"),
      BioStats("biosAcknowledged", labelPrefix = "bios acknowledged"),
      BioStats("biosAcknowledgedPartial", labelPrefix = "bios acknowledged partial"),
      # Current number of bios in progress
      BioStats("biosInProgress", labelPrefix = "bios in progress"),
      # Memory usage stats.
      MemoryUsage("memoryUsage", labelPrefix = "KVDO module"),
      # The statistics for the UDS index
      IndexStatistics("index"),
    ], procFile="kernel_stats", procRoot="vdo", **kwargs)

  statisticsVersion = 30

  def sample(self, device):
    sample = super(KernelStatistics, self).sample(device)
    if ((sample.getStat("version") != KernelStatistics.statisticsVersion) or (sample.getStat("releaseVersion") != CURRENT_RELEASE_VERSION_NUMBER)):
      raise Exception("KernelStatistics version mismatch")
    return sample

