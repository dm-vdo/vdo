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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoDebugMetadata.c#12 $
 */

#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "logger.h"
#include "memoryAlloc.h"
#include "stringUtils.h"
#include "syscalls.h"

#include "blockMapInternals.h"
#include "numUtils.h"
#include "packedRecoveryJournalBlock.h"
#include "recoveryJournalEntry.h"
#include "recoveryJournalInternals.h"
#include "recoveryUtils.h"
#include "referenceBlock.h"
#include "slabDepotInternals.h"
#include "slabJournalInternals.h"
#include "slabSummaryInternals.h"
#include "types.h"
#include "vdo.h"
#include "vdoInternal.h"
#include "vdoLoad.h"
#include "volumeGeometry.h"

#include "fileLayer.h"

static const char usageString[]
  = "[--help] [--pbn=<pbn>] [--searchLBN=<lbn>] [--version] filename";

static const char helpString[] =
  "vdoDebugMetadata - load a metadata dump of a VDO device\n"
  "\n"
  "SYNOPSIS\n"
  "  vdoDebugMetadata [--pbn=<pbn>] [--searchLBN=<lbn>] <filename>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdoDebugMetadata loads the metadata regions dumped by vdoDumpMetadata.\n"
  "  It should be run under GDB, with a breakpoint on the function\n"
  "  doNothing.\n"
  "\n"
  "  Variables vdo, slabSummary, slabs, and recoveryJournal are\n"
  "  available, providing access to the VDO super block state, the slab\n"
  "  summary blocks, all slab journal and reference blocks per slab,\n"
  "  and all recovery journal blocks.\n"
  "\n"
  "  Please note that this tool does not provide access to block map pages.\n"
  "\n"
  "  Any --pbn argument(s) will print the slab journal entries for the\n"
  "  given PBN(s).\n"
  "\n"
  "  Any --searchLBN argument(s) will print the recovery journal entries\n"
  "  for the given LBN(s). This includes PBN, increment/decrement, mapping\n"
  "  state, recovery journal position information, and whether the \n"
  "  recovery journal block is valid.\n"
  "\n";

static struct option options[] = {
  { "help",      no_argument,       NULL, 'h' },
  { "pbn",       required_argument, NULL, 'p' },
  { "searchLBN", required_argument, NULL, 's' },
  { "version",   no_argument,       NULL, 'V' },
  { NULL,        0,                 NULL,  0  },
};

typedef struct {
  PackedSlabJournalBlock  **slabJournalBlocks;
  PackedReferenceBlock    **referenceBlocks;
} SlabState;

typedef struct {
  RecoveryBlockHeader  header;
  PackedJournalSector *sectors[SECTORS_PER_BLOCK];
} UnpackedJournalBlock;

static VDO                   *vdo             = NULL;
static SlabSummaryEntry     **slabSummary     = NULL;
static SlabCount              slabCount       = 0;
static SlabState             *slabs           = NULL;
static UnpackedJournalBlock  *recoveryJournal = NULL;
static char                  *rawJournalBytes = NULL;

static PhysicalBlockNumber    nextBlock;
static const SlabConfig      *slabConfig      = NULL;

static PhysicalBlockNumber   *pbns            = NULL;
static uint8_t                pbnCount        = 0;

static LogicalBlockNumber    *searchLBNs      = NULL;
static uint8_t                searchLBNCount  = 0;

enum {
  MAX_PBNS        = 255,
  MAX_SEARCH_LBNS = 255,
};

/**
 * Explain how this program is used.
 *
 * @param progname           Name of this program
 * @param usageOptionString  Explanation
 **/
static void usage(const char *progname, const char *usageOptionsString)
{
  fprintf(stderr, "Usage: %s %s\n", progname, usageOptionsString);
  exit(1);
}

/**
 * Get the filename (or "help") from the input arguments.
 * Print command usage if arguments are wrong.
 *
 * @param [in]  argc       Number of input arguments
 * @param [in]  argv       Array of input arguments
 * @param [out] filename   Name of this VDO's file or block device
 *
 * @return VDO_SUCCESS or some error.
 **/
static int processArgs(int argc, char *argv[], char **filename)
{
  int      c;
  char    *optionString = "hp:s:V";
  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    if (c == (int) 'h') {
      printf("%s", helpString);
      exit(0);
    }

    if (c == (int) 'V') {
      printf("%s version is: %s\n", argv[0], CURRENT_VERSION);
      exit(0);
    }

    if (c == (int) 'p') {
      // Limit to 255 PBNs for now.
      if (pbnCount == MAX_PBNS) {
        errx(1, "Cannot specify more than %u PBNs", MAX_PBNS);
      }

      int result = parseUint64(optarg, &pbns[pbnCount++]);
      if (result != VDO_SUCCESS) {
        warnx("Cannot parse PBN as a number");
        usage(argv[0], usageString);
      }
    }

    if (c == (int) 's') {
      // Limit to 255 search LBNs for now.
      if (pbnCount == MAX_SEARCH_LBNS) {
        errx(1, "Cannot specify more than %u search LBNs", MAX_SEARCH_LBNS);
      }

      int result = parseUint64(optarg, &searchLBNs[searchLBNCount++]);
      if (result != VDO_SUCCESS) {
        warnx("Cannot parse search LBN as a number");
        usage(argv[0], usageString);
      }
    }
  }

  // Explain usage and exit.
  if (optind != (argc - 1)) {
    usage(argv[0], usageString);
  }

  *filename = argv[optind];

  return VDO_SUCCESS;
}

/**
 * This function provides an easy place to set a breakpoint.
 **/
__attribute__((noinline)) static void doNothing(void)
{
  __asm__("");
}

/**
 * Read blocks from the current position.
 *
 * @param [in]  count   How many blocks to read
 * @param [out] buffer  The buffer to read into
 *
 * @return VDO_SUCCESS or an error
 **/
static int readBlocks(BlockCount count, char *buffer)
{
  int result = vdo->layer->reader(vdo->layer, nextBlock, count, buffer, NULL);
  if (result != VDO_SUCCESS) {
    return result;
  }
  nextBlock += count;
  return result;
}

/**
 * Free a single slab state
 *
 * @param statePtr      A pointer to the state to free
 **/
static void freeState(SlabState *state)
{
  if (state == NULL) {
    return;
  }

  if (state->slabJournalBlocks != NULL) {
    for (BlockCount i = 0; i < slabConfig->slabJournalBlocks; i++) {
      FREE(state->slabJournalBlocks[i]);
      state->slabJournalBlocks[i] = NULL;
    }
  }

  if (state->referenceBlocks != NULL) {
    for (BlockCount i = 0; i < slabConfig->referenceCountBlocks; i++) {
      FREE(state->referenceBlocks[i]);
      state->referenceBlocks[i] = NULL;
    }
  }

  FREE(state->slabJournalBlocks);
  FREE(state->referenceBlocks);
}

/**
 * Allocate a single slab state.
 *
 * @param [out] statePtr  Where to store the allocated state.
 **/
static int allocateState(SlabState *state)
{
  int result = ALLOCATE(slabConfig->slabJournalBlocks,
                        PackedSlabJournalBlock *, __func__,
                        &state->slabJournalBlocks);
  if (result != VDO_SUCCESS) {
    freeState(state);
    return result;
  }

  result = ALLOCATE(slabConfig->referenceCountBlocks, PackedReferenceBlock *,
                    __func__, &state->referenceBlocks);
  if (result != VDO_SUCCESS) {
    freeState(state);
    return result;
  }

  PhysicalLayer *layer = vdo->layer;
  for (BlockCount i = 0; i < slabConfig->referenceCountBlocks; i++) {
    char *buffer;
    result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
                                     "reference count block", &buffer);
    if (result != VDO_SUCCESS) {
      freeState(state);
      return result;
    }
    state->referenceBlocks[i] = (PackedReferenceBlock *) buffer;
  }

  for (BlockCount i = 0; i < slabConfig->slabJournalBlocks; i++) {
    char *buffer;
    result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
                                     "slab journal block", &buffer);
    if (result != VDO_SUCCESS) {
      freeState(state);
      return result;
    }
    state->slabJournalBlocks[i] = (PackedSlabJournalBlock *) buffer;
  }
  return result;
}

/**
 * Allocate sufficient space to read the metadata dump.
 **/
static int allocateMetadataSpace(void)
{
  SlabDepot *depot          = vdo->depot;
  SlabCount  totalSlabCount = calculateSlabCount(depot);
  slabConfig = getSlabConfig(depot);

  int result = ALLOCATE(totalSlabCount, SlabState, __func__, &slabs);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %u slab state pointers", slabCount);
  }

  while (slabCount < totalSlabCount) {
    result = allocateState(&slabs[slabCount]);
    if (result != VDO_SUCCESS) {
      errx(1, "Could not allocate slab state for slab %u", slabCount);
    }

    slabCount++;
  }

  PhysicalLayer *layer = vdo->layer;
  size_t journalBytes = vdo->config.recoveryJournalSize * VDO_BLOCK_SIZE;
  result = layer->allocateIOBuffer(layer, journalBytes,
                                   "recovery journal", &rawJournalBytes);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64" bytes for the journal",
         journalBytes);
  }

  result = ALLOCATE(vdo->config.recoveryJournalSize, UnpackedJournalBlock,
                    __func__, &recoveryJournal);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64 " journal block structures",
         vdo->config.recoveryJournalSize);
  }

  result = ALLOCATE(getSlabSummarySize(VDO_BLOCK_SIZE), SlabSummaryEntry *,
                    __func__, &slabSummary);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64 " slab summary block pointers",
         getSlabSummarySize(VDO_BLOCK_SIZE));
  }

  for (BlockCount i = 0; i < getSlabSummarySize(VDO_BLOCK_SIZE); i++) {
    char *buffer;
    result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
                                     "slab summary block", &buffer);
    if (result != VDO_SUCCESS) {
      errx(1, "Could not allocate slab summary block %" PRIu64, i);
    }
    slabSummary[i] = (SlabSummaryEntry *) buffer;
  }
  return result;
}

/**
 * Free the allocations from allocateMetadataSpace().
 **/
static void freeMetadataSpace(void)
{
  if (slabs != NULL) {
    for (SlabCount i = 0; i < slabCount; i++) {
      freeState(&slabs[i]);
    }
  }

  FREE(slabs);
  slabs = NULL;

  FREE(rawJournalBytes);
  rawJournalBytes = NULL;

  FREE(recoveryJournal);
  recoveryJournal = NULL;

  if (slabSummary != NULL) {
    for (BlockCount i = 0; i < getSlabSummarySize(VDO_BLOCK_SIZE); i++) {
      FREE(slabSummary[i]);
      slabSummary[i] = NULL;
    }
  }

  FREE(slabSummary);
  slabSummary = NULL;
}

/**
 * Read the metadata into the appropriate places.
 **/
static void readMetadata(void)
{
  /**
   * The dump tool dumps the whole block map of whatever size, or some LBNs,
   * or nothing, at the beginning of the dump. This tool doesn't currently know
   * how to read the block map, so we figure out how many other metadata blocks
   * there are, then skip back from the end of the file to the beginning of
   * that metadata.
   **/
  BlockCount metadataBlocksPerSlab
    = (slabConfig->referenceCountBlocks + slabConfig->slabJournalBlocks);
  BlockCount totalNonBlockMapMetadataBlocks
    = ((metadataBlocksPerSlab * slabCount)
       + vdo->config.recoveryJournalSize
       + getSlabSummarySize(VDO_BLOCK_SIZE));

  nextBlock
    = (vdo->layer->getBlockCount(vdo->layer) - totalNonBlockMapMetadataBlocks);

  for (SlabCount i = 0; i < slabCount; i++) {
    SlabState *slab = &slabs[i];
    for (BlockCount j = 0; j < slabConfig->referenceCountBlocks; j++) {
      int result = readBlocks(1, (char *) slab->referenceBlocks[j]);
      if (result != VDO_SUCCESS) {
        errx(1, "Could not read reference block %" PRIu64 " for slab %u", j,
             i);
      }
    }

    for (BlockCount j = 0; j < slabConfig->slabJournalBlocks; j++) {
      int result = readBlocks(1, (char *) slab->slabJournalBlocks[j]);
      if (result != VDO_SUCCESS) {
        errx(1, "Could not read slab journal block %" PRIu64 " for slab %u",
             j, i);
      }
    }
  }

  int result = readBlocks(vdo->config.recoveryJournalSize, rawJournalBytes);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not read recovery journal");
  }

  for (BlockCount i = 0; i < vdo->config.recoveryJournalSize; i++) {
    UnpackedJournalBlock *block = &recoveryJournal[i];
    PackedJournalHeader *packedHeader
      = (PackedJournalHeader *) &rawJournalBytes[i * VDO_BLOCK_SIZE];
    unpackRecoveryBlockHeader(packedHeader, &block->header);
    for (uint8_t sector = 1; sector < SECTORS_PER_BLOCK; sector++) {
      block->sectors[sector] = getJournalBlockSector(packedHeader, sector);
    }
  }

  for (BlockCount i = 0; i < getSlabSummarySize(VDO_BLOCK_SIZE); i++) {
    readBlocks(1, (char *) slabSummary[i]);
  }
}

/**
 *  Search slab journal for PBNs.
 **/
static void findSlabJournalEntries(PhysicalBlockNumber pbn)
{
  SlabDepot *depot = vdo->depot;
  if ((pbn < depot->firstBlock) || (pbn > depot->lastBlock)) {
    printf("PBN %" PRIu64 " out of range; skipping.\n", pbn);
    return;
  }

  BlockCount      offset          = pbn - depot->firstBlock;
  SlabCount       slabNumber      = offset >> depot->slabSizeShift;
  uint64_t        slabOffsetMask  = (1 << depot->slabSizeShift) - 1;
  SlabBlockNumber slabOffset      = offset & slabOffsetMask;

  printf("PBN %" PRIu64 " is offset %d in slab %d\n",
         pbn, slabOffset, slabNumber);
  for (BlockCount i = 0; i < depot->slabConfig.slabJournalBlocks; i++) {
    PackedSlabJournalBlock *block = slabs[slabNumber].slabJournalBlocks[i];
    JournalEntryCount entryCount
      = getUInt16LE(block->header.fields.entryCount);
    for (JournalEntryCount entryIndex = 0;
         entryIndex < entryCount;
         entryIndex++) {
      SlabJournalEntry entry = decodeSlabJournalEntry(block, entryIndex);
      if (slabOffset == entry.sbn) {
        printf("PBN %" PRIu64 " (%" PRIu64 ", %d) %s\n",
               pbn, getUInt64LE(block->header.fields.sequenceNumber),
               entryIndex, getJournalOperationName(entry.operation));
      }
    }
  }
}

/**
 * Determine whether the given header describes a valid block for the
 * given journal, even if it is older than the last successful recovery
 * or reformat. A block is not "relevant" if it is unformatted, or has a
 * different nonce value.  Use this for cases where it would not be
 * appropriate to use isValidRecoveryJournalBlock because we do want to
 * consider blocks with other recoveryCount values.
 *
 * @param header   The unpacked block header to check
 *
 * @return <code>True</code> if the header is valid
 **/
__attribute__((warn_unused_result))
static inline bool isBlockFromJournal(const RecoveryBlockHeader *header)
{
  return ((header->metadataType == VDO_METADATA_RECOVERY_JOURNAL)
          && (header->nonce == vdo->recoveryJournal->nonce));
}

/**
 * Determine whether the sequence number is possible for the given
 * offset.  Similar to isCongruentRecoveryJournalBlock(), but does not
 * run isValidRecoveryJournalBlock().
 *
 * @param header   The unpacked block header to check
 * @param offset   An offset indicating where the block was in the journal
 *
 * @return <code>True</code> if the sequence number is possible
 **/
__attribute__((warn_unused_result))
static inline
bool isSequenceNumberPossibleForOffset(const RecoveryBlockHeader *header,
                                       PhysicalBlockNumber        offset)
{
  PhysicalBlockNumber expectedOffset
    = getRecoveryJournalBlockNumber(vdo->recoveryJournal,
                                    header->sequenceNumber);
  return (expectedOffset == offset);
}

/**
 *  Search recovery journal for PBNs belonging to the given LBN.
 **/
static void findRecoveryJournalEntries(LogicalBlockNumber lbn)
{
  BlockMapSlot desiredSlot = (BlockMapSlot) {
    .pbn  = computePageNumber(lbn),
    .slot = computeSlot(lbn),
  };
  for (BlockCount i = 0; i < vdo->config.recoveryJournalSize; i++) {
    UnpackedJournalBlock block = recoveryJournal[i];

    for (SectorCount j = 1; j < SECTORS_PER_BLOCK; j++) {
      const PackedJournalSector *sector = block.sectors[j];

      for (JournalEntryCount k = 0; k < sector->entryCount; k++) {
        RecoveryJournalEntry entry
          = unpackRecoveryJournalEntry(&sector->entries[k]);

        if ((desiredSlot.pbn == entry.slot.pbn)
            && (desiredSlot.slot == entry.slot.slot)) {
          bool isValidJournalBlock = isBlockFromJournal(&block.header);
          bool isSequenceNumberPossible
            = isSequenceNumberPossibleForOffset(&block.header, i);
          bool isSectorValid
            = isValidRecoveryJournalSector(&block.header, sector);

          printf("found LBN %" PRIu64 " at offset %" PRIu64
                 " (block %svalid, sequence number %" PRIu64 " %spossible), "
                 "sector %u (sector %svalid), entry %u "
                 ": PBN %" PRIu64 ", %s, mappingState %u\n",
                 lbn, i, (isValidJournalBlock ? "" : "not "),
                 block.header.sequenceNumber,
                 (isSequenceNumberPossible ? "" : "not "),
                 j, (isSectorValid ? "" : "not "), k,
                 entry.mapping.pbn, getJournalOperationName(entry.operation),
                 entry.mapping.state);
        }
      }
    }
  }
}

/**
 * Load from a dump file.
 *
 * @param [in]  filename  The file name
 * @param [out] vdoPtr    A pointer to hold the VDO
 *
 * @return VDO_SUCCESS or an error code
 **/
__attribute__((warn_unused_result))
static int readVDOFromDump(const char *filename, VDO **vdoPtr)
{
  PhysicalLayer *layer;
  int result = makeReadOnlyFileLayer(filename, &layer);

  if (result != VDO_SUCCESS) {
    char errBuf[ERRBUF_SIZE];
    warnx("Failed to make FileLayer from '%s' with %s",
          filename, stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  // Load the geometry and tweak it to match the dump.
  VolumeGeometry geometry;
  result = loadVolumeGeometry(layer, &geometry);
  if (result != VDO_SUCCESS) {
    layer->destroy(&layer);
    char errBuf[ERRBUF_SIZE];
    warnx("VDO geometry read failed for '%s' with %s",
          filename, stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }
  geometry.regions[DATA_REGION].startBlock = 1;

  // Create the VDO.
  VDO *vdo;
  result = loadVDOSuperblock(layer, &geometry, false, NULL, &vdo);
  if (result != VDO_SUCCESS) {
    layer->destroy(&layer);
    char errBuf[ERRBUF_SIZE];
    warnx("VDO load failed for '%s' with %s",
          filename, stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  *vdoPtr = vdo;
  return VDO_SUCCESS;
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  char *filename;
  int result = ALLOCATE(MAX_PBNS, PhysicalBlockNumber, __func__, &pbns);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64 " bytes",
         sizeof(PhysicalBlockNumber) * MAX_PBNS);
  }

  result = ALLOCATE(MAX_SEARCH_LBNS, LogicalBlockNumber, __func__,
                    &searchLBNs);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64 " bytes",
         sizeof(LogicalBlockNumber) * MAX_SEARCH_LBNS);
  }

  result = processArgs(argc, argv, &filename);
  if (result != VDO_SUCCESS) {
    exit(1);
  }

  static char errBuf[ERRBUF_SIZE];

  result = registerStatusCodes();
  if (result != UDS_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         stringError(result, errBuf, ERRBUF_SIZE));
  }

  openLogger();

  result = readVDOFromDump(filename, &vdo);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not load VDO from '%s': %s",
         filename, stringError(result, errBuf, ERRBUF_SIZE));
  }

  allocateMetadataSpace();

  readMetadata();

  // Print the nonce for this dump.
  printf("Nonce value: %" PRIu64 "\n", vdo->nonce);

  // For any PBNs specified, process them.
  for (uint8_t i = 0; i < pbnCount; i++) {
    findSlabJournalEntries(pbns[i]);
  }

  // Process any search LBNs.
  for (uint8_t i = 0; i < searchLBNCount; i++) {
    findRecoveryJournalEntries(searchLBNs[i]);
  }

  // This is a great line for a GDB breakpoint.
  doNothing();

  // If someone runs the program manually, tell them to use GDB.
  if ((pbnCount == 0) && (searchLBNCount == 0)) {
    printf("%s", helpString);
  }

  freeMetadataSpace();
  PhysicalLayer *layer = vdo->layer;
  freeVDO(&vdo);
  layer->destroy(&layer);
  exit(result);
}
