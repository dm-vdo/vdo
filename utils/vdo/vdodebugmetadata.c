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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoDebugMetadata.c#37 $
 */

#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"
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
#include "statusCodes.h"
#include "types.h"
#include "vdo.h"
#include "vdoInternal.h"
#include "volumeGeometry.h"

#include "fileLayer.h"
#include "vdoVolumeUtils.h"

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
  struct packed_slab_journal_block **slabJournalBlocks;
  struct packed_reference_block    **referenceBlocks;
} SlabState;

typedef struct {
  struct recovery_block_header  header;
  struct packed_journal_sector *sectors[SECTORS_PER_BLOCK];
} UnpackedJournalBlock;

static struct vdo                     *vdo             = NULL;
static struct slab_summary_entry     **slabSummary     = NULL;
static slab_count_t                    slabCount       = 0;
static SlabState                      *slabs           = NULL;
static UnpackedJournalBlock           *recoveryJournal = NULL;
static char                           *rawJournalBytes = NULL;

static physical_block_number_t         nextBlock;
static const struct slab_config       *slabConfig      = NULL;

static physical_block_number_t        *pbns            = NULL;
static uint8_t                         pbnCount        = 0;

static logical_block_number_t         *searchLBNs      = NULL;
static uint8_t                         searchLBNCount  = 0;

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
static int readBlocks(block_count_t count, char *buffer)
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
    for (block_count_t i = 0; i < slabConfig->slab_journal_blocks; i++) {
      FREE(state->slabJournalBlocks[i]);
      state->slabJournalBlocks[i] = NULL;
    }
  }

  if (state->referenceBlocks != NULL) {
    for (block_count_t i = 0; i < slabConfig->reference_count_blocks; i++) {
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
  int result = ALLOCATE(slabConfig->slab_journal_blocks,
                        struct packed_slab_journal_block *, __func__,
                        &state->slabJournalBlocks);
  if (result != VDO_SUCCESS) {
    freeState(state);
    return result;
  }

  result = ALLOCATE(slabConfig->reference_count_blocks,
                    struct packed_reference_block *,
                    __func__, &state->referenceBlocks);
  if (result != VDO_SUCCESS) {
    freeState(state);
    return result;
  }

  PhysicalLayer *layer = vdo->layer;
  for (block_count_t i = 0; i < slabConfig->reference_count_blocks; i++) {
    char *buffer;
    result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
                                     "reference count block", &buffer);
    if (result != VDO_SUCCESS) {
      freeState(state);
      return result;
    }
    state->referenceBlocks[i] = (struct packed_reference_block *) buffer;
  }

  for (block_count_t i = 0; i < slabConfig->slab_journal_blocks; i++) {
    char *buffer;
    result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
                                     "slab journal block", &buffer);
    if (result != VDO_SUCCESS) {
      freeState(state);
      return result;
    }
    state->slabJournalBlocks[i] = (struct packed_slab_journal_block *) buffer;
  }
  return result;
}

/**
 * Allocate sufficient space to read the metadata dump.
 **/
static int allocateMetadataSpace(void)
{
  struct slab_depot *depot          = vdo->depot;
  slab_count_t       totalSlabCount = calculate_slab_count(depot);
  slabConfig                        = get_slab_config(depot);

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
  size_t journalBytes = vdo->config.recovery_journal_size * VDO_BLOCK_SIZE;
  result = layer->allocateIOBuffer(layer, journalBytes,
                                   "recovery journal", &rawJournalBytes);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64" bytes for the journal",
         journalBytes);
  }

  result = ALLOCATE(vdo->config.recovery_journal_size, UnpackedJournalBlock,
                    __func__, &recoveryJournal);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64 " journal block structures",
         vdo->config.recovery_journal_size);
  }

  result = ALLOCATE(get_slab_summary_size(VDO_BLOCK_SIZE),
                    struct slab_summary_entry *,
                    __func__, &slabSummary);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64 " slab summary block pointers",
         get_slab_summary_size(VDO_BLOCK_SIZE));
  }

  for (block_count_t i = 0; i < get_slab_summary_size(VDO_BLOCK_SIZE); i++) {
    char *buffer;
    result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
                                     "slab summary block", &buffer);
    if (result != VDO_SUCCESS) {
      errx(1, "Could not allocate slab summary block %" PRIu64, i);
    }
    slabSummary[i] = (struct slab_summary_entry *) buffer;
  }
  return result;
}

/**
 * Free the allocations from allocateMetadataSpace().
 **/
static void freeMetadataSpace(void)
{
  if (slabs != NULL) {
    for (slab_count_t i = 0; i < slabCount; i++) {
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
    for (block_count_t i = 0; i < get_slab_summary_size(VDO_BLOCK_SIZE); i++) {
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
  block_count_t metadataBlocksPerSlab
    = (slabConfig->reference_count_blocks + slabConfig->slab_journal_blocks);
  block_count_t totalNonBlockMapMetadataBlocks
    = ((metadataBlocksPerSlab * slabCount)
       + vdo->config.recovery_journal_size
       + get_slab_summary_size(VDO_BLOCK_SIZE));

  nextBlock
    = (vdo->layer->getBlockCount(vdo->layer) - totalNonBlockMapMetadataBlocks);

  for (slab_count_t i = 0; i < slabCount; i++) {
    SlabState *slab = &slabs[i];
    for (block_count_t j = 0; j < slabConfig->reference_count_blocks; j++) {
      int result = readBlocks(1, (char *) slab->referenceBlocks[j]);
      if (result != VDO_SUCCESS) {
        errx(1, "Could not read reference block %" PRIu64 " for slab %u", j,
             i);
      }
    }

    for (block_count_t j = 0; j < slabConfig->slab_journal_blocks; j++) {
      int result = readBlocks(1, (char *) slab->slabJournalBlocks[j]);
      if (result != VDO_SUCCESS) {
        errx(1, "Could not read slab journal block %" PRIu64 " for slab %u",
             j, i);
      }
    }
  }

  int result = readBlocks(vdo->config.recovery_journal_size, rawJournalBytes);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not read recovery journal");
  }

  for (block_count_t i = 0; i < vdo->config.recovery_journal_size; i++) {
    UnpackedJournalBlock *block = &recoveryJournal[i];
    union packed_journal_header *packedHeader
      = (union packed_journal_header *) &rawJournalBytes[i * VDO_BLOCK_SIZE];
    unpack_recovery_block_header(packedHeader, &block->header);
    for (uint8_t sector = 1; sector < SECTORS_PER_BLOCK; sector++) {
      block->sectors[sector] = get_journal_block_sector(packedHeader, sector);
    }
  }

  for (block_count_t i = 0; i < get_slab_summary_size(VDO_BLOCK_SIZE); i++) {
    readBlocks(1, (char *) slabSummary[i]);
  }
}

/**
 *  Search slab journal for PBNs.
 **/
static void findSlabJournalEntries(physical_block_number_t pbn)
{
  struct slab_depot *depot = vdo->depot;
  if ((pbn < depot->first_block) || (pbn > depot->last_block)) {
    printf("PBN %" PRIu64 " out of range; skipping.\n", pbn);
    return;
  }

  block_count_t     offset          = pbn - depot->first_block;
  slab_count_t      slabNumber      = offset >> depot->slab_size_shift;
  uint64_t          slabOffsetMask  = (1 << depot->slab_size_shift) - 1;
  slab_block_number slabOffset      = offset & slabOffsetMask;

  printf("PBN %" PRIu64 " is offset %d in slab %d\n",
         pbn, slabOffset, slabNumber);
  for (block_count_t i = 0; i < depot->slab_config.slab_journal_blocks; i++) {
    struct packed_slab_journal_block *block
      = slabs[slabNumber].slabJournalBlocks[i];
    JournalEntryCount entryCount
      = get_unaligned_le16(block->header.fields.entry_count);
    for (JournalEntryCount entryIndex = 0;
         entryIndex < entryCount;
         entryIndex++) {
      struct slab_journal_entry entry
        = decode_slab_journal_entry(block, entryIndex);
      if (slabOffset == entry.sbn) {
        printf("PBN %" PRIu64 " (%" PRIu64 ", %d) %s\n",
               pbn, get_unaligned_le64(block->header.fields.sequence_number),
               entryIndex, get_journal_operation_name(entry.operation));
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
static inline bool __must_check
isBlockFromJournal(const struct recovery_block_header *header)
{
  return ((header->metadata_type == VDO_METADATA_RECOVERY_JOURNAL)
          && (header->nonce == vdo->recovery_journal->nonce));
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
static inline bool __must_check
isSequenceNumberPossibleForOffset(const struct recovery_block_header *header,
				  physical_block_number_t             offset)
{
  physical_block_number_t expectedOffset
    = get_recovery_journal_block_number(vdo->recovery_journal,
                                        header->sequence_number);
  return (expectedOffset == offset);
}

/**
 *  Search recovery journal for PBNs belonging to the given LBN.
 **/
static void findRecoveryJournalEntries(logical_block_number_t lbn)
{
  struct block_map_slot desiredSlot = (struct block_map_slot) {
    .pbn  = compute_page_number(lbn),
    .slot = compute_slot(lbn),
  };
  for (block_count_t i = 0; i < vdo->config.recovery_journal_size; i++) {
    UnpackedJournalBlock block = recoveryJournal[i];

    for (sector_count_t j = 1; j < SECTORS_PER_BLOCK; j++) {
      const struct packed_journal_sector *sector = block.sectors[j];

      for (JournalEntryCount k = 0; k < sector->entry_count; k++) {
        struct recovery_journal_entry entry
          = unpack_recovery_journal_entry(&sector->entries[k]);

        if ((desiredSlot.pbn == entry.slot.pbn)
            && (desiredSlot.slot == entry.slot.slot)) {
          bool isValidJournalBlock = isBlockFromJournal(&block.header);
          bool isSequenceNumberPossible
            = isSequenceNumberPossibleForOffset(&block.header, i);
          bool isSectorValid
            = is_valid_recovery_journal_sector(&block.header, sector);

          printf("found LBN %" PRIu64 " at offset %" PRIu64
                 " (block %svalid, sequence number %" PRIu64 " %spossible), "
                 "sector %u (sector %svalid), entry %u "
                 ": PBN %" PRIu64 ", %s, mappingState %u\n",
                 lbn, i, (isValidJournalBlock ? "" : "not "),
                 block.header.sequence_number,
                 (isSequenceNumberPossible ? "" : "not "),
                 j, (isSectorValid ? "" : "not "), k,
                 entry.mapping.pbn, get_journal_operation_name(entry.operation),
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
static int __must_check
readVDOFromDump(const char *filename, struct vdo **vdoPtr)
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
  struct volume_geometry geometry;
  result = load_volume_geometry(layer, &geometry);
  if (result != VDO_SUCCESS) {
    layer->destroy(&layer);
    char errBuf[ERRBUF_SIZE];
    warnx("VDO geometry read failed for '%s' with %s",
          filename, stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }
  geometry.regions[DATA_REGION].start_block = 1;

  // Create the VDO.
  struct vdo *vdo;
  result = load_vdo_superblock(layer, &geometry, false, &vdo);
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
  static char errBuf[ERRBUF_SIZE];

  int result = register_status_codes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         stringError(result, errBuf, ERRBUF_SIZE));
  }

  char *filename;
  result = ALLOCATE(MAX_PBNS, physical_block_number_t, __func__, &pbns);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64 " bytes",
         sizeof(physical_block_number_t) * MAX_PBNS);
  }

  result = ALLOCATE(MAX_SEARCH_LBNS, logical_block_number_t, __func__,
                    &searchLBNs);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64 " bytes",
         sizeof(logical_block_number_t) * MAX_SEARCH_LBNS);
  }

  result = processArgs(argc, argv, &filename);
  if (result != VDO_SUCCESS) {
    exit(1);
  }

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
  free_vdo(&vdo);
  layer->destroy(&layer);
  exit(result);
}
