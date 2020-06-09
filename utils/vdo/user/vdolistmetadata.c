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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoListMetadata.c#3 $
 */

#include <err.h>
#include <getopt.h>

#include "syscalls.h"

#include "blockMapInternals.h"
#include "fixedLayout.h"
#include "slabDepotInternals.h"
#include "slabSummary.h"
#include "types.h"
#include "vdoInternal.h"
#include "vdoLayout.h"

#include "vdoVolumeUtils.h"

static const char usageString[] = "[--help] [--version] <vdoBackingDevice>";

static const char helpString[] =
  "vdoListMetadata - list the metadata regions on a VDO device\n"
  "\n"
  "SYNOPSIS\n"
  "  vdoListMetadata <vdoBackingDevice>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdoListMetadata lists the metadata regions of a VDO device\n"
  "  as ranges of block numbers. Each range is on a separate line\n"
  "  of the form:\n"
  "    startBlock .. endBlock: label\n"
  "  Both endpoints are included in the range, and are the zero-based\n"
  "  indexes of 4KB VDO metadata blocks on the backing device.\n"
  "\n";

static struct option options[] = {
  { "help",    no_argument, NULL, 'h' },
  { "version", no_argument, NULL, 'V' },
  { NULL,      0,           NULL,  0  },
};

static char *vdoBackingName = NULL;
static VDO  *vdo            = NULL;

/**
 * Explain how this command-line tool is used.
 *
 * @param programName  Name of this program
 * @param usageString  Multi-line explanation
 **/
static void usage(const char *programName)
{
  errx(1, "Usage: %s %s\n", programName, usageString);
}

/**
 * Parse the arguments passed; print command usage if arguments are wrong.
 *
 * @param argc  Number of input arguments
 * @param argv  Array of input arguments
 **/
static void processArgs(int argc, char *argv[])
{
  int c;
  while ((c = getopt_long(argc, argv, "hV", options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", helpString);
      exit(0);

    case 'V':
      printf("%s version is: %s\n", argv[0], CURRENT_VERSION);
      exit(0);

    default:
      usage(argv[0]);
      break;
    }
  }

  // Explain usage and exit
  if (optind != (argc - 1)) {
    usage(argv[0]);
  }

  vdoBackingName = argv[optind++];
}

/**
 * List a range of metadata blocks on stdout.
 *
 * @param label       The type of metadata
 * @param startBlock  The block to start at in the VDO backing device
 * @param count       The number of metadata blocks in the range
 **/
static void listBlocks(const char          *label,
                       PhysicalBlockNumber  startBlock,
                       BlockCount           count)
{
  printf("%ld .. %ld: %s\n", startBlock, startBlock + count - 1, label);
}

/**********************************************************************/
static void listGeometryBlock(void)
{
  // The geometry block is a single block at the start of the volume.
  listBlocks("geometry block", 0, 1);
}

/**********************************************************************/
static void listIndex(void)
{
  // The index is all blocks from the geometry block to the super block,
  // exclusive.
  listBlocks("index", 1, getFirstBlockOffset(vdo) - 1);
}

/**********************************************************************/
static void listSuperBlock(void)
{
  // The SuperBlock is a single block at the start of the data region.
  listBlocks("super block", getFirstBlockOffset(vdo), 1);
}

/**********************************************************************/
static void listBlockMap(void)
{
  BlockMap *map = getBlockMap(vdo);
  if (map->flatPageCount > 0) {
    listBlocks("flat block map", BLOCK_MAP_FLAT_PAGE_ORIGIN,
               map->flatPageCount);
  }

  if (map->rootCount > 0) {
    listBlocks("block map tree roots", map->rootOrigin, map->rootCount);
  }
}

/**********************************************************************/
static void listSlab(SlabCount         slab,
                     const SlabConfig *slabConfig)
{
  PhysicalBlockNumber slabOrigin
    = vdo->depot->firstBlock + (slab * vdo->config.slabSize);

  // List the slab's reference count blocks.
  char buffer[64];
  sprintf(buffer, "slab %u reference blocks", slab);
  listBlocks(buffer, slabOrigin + slabConfig->dataBlocks,
             slabConfig->referenceCountBlocks);

  // List the slab's journal blocks.
  sprintf(buffer, "slab %u journal", slab);
  listBlocks(buffer, getSlabJournalStartBlock(slabConfig, slabOrigin),
             slabConfig->slabJournalBlocks);
}

/**********************************************************************/
static void listSlabs(void)
{
  SlabDepot        *depot      = vdo->depot;
  SlabCount         slabCount  = calculateSlabCount(depot);
  const SlabConfig *slabConfig = getSlabConfig(depot);
  for (SlabCount slab = 0; slab < slabCount; slab++) {
    listSlab(slab, slabConfig);
  }
}

/**********************************************************************/
static void listRecoveryJournal(void)
{
  const Partition *partition = getVDOPartition(vdo->layout,
                                               RECOVERY_JOURNAL_PARTITION);
  listBlocks("recovery journal", getFixedLayoutPartitionOffset(partition),
             vdo->config.recoveryJournalSize);
}

/**********************************************************************/
static void listSlabSummary(void)
{
  const Partition *partition = getVDOPartition(vdo->layout,
                                               SLAB_SUMMARY_PARTITION);
  listBlocks("slab summary", getFixedLayoutPartitionOffset(partition),
             getSlabSummarySize(VDO_BLOCK_SIZE));
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  processArgs(argc, argv);

  // Read input VDO, without validating its config.
  int result = readVDOWithoutValidation(vdoBackingName, &vdo);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not load VDO from '%s'", vdoBackingName);
  }

  listGeometryBlock();
  listIndex();
  listSuperBlock();
  listBlockMap();
  listSlabs();
  listRecoveryJournal();
  listSlabSummary();

  freeVDOFromFile(&vdo);
  exit(0);
}