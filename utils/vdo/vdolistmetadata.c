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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoListMetadata.c#22 $
 */

#include <err.h>
#include <getopt.h>

#include "errors.h"
#include "syscalls.h"

#include "blockMapInternals.h"
#include "fixedLayout.h"
#include "slabDepotInternals.h"
#include "slabJournalFormat.h"
#include "slabSummary.h"
#include "statusCodes.h"
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
static struct vdo *vdo      = NULL;

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
static void listBlocks(const char              *label,
                       physical_block_number_t  startBlock,
                       block_count_t            count)
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
  listBlocks("index", 1, get_first_block_offset(vdo) - 1);
}

/**********************************************************************/
static void listSuperBlock(void)
{
  // The SuperBlock is a single block at the start of the data region.
  listBlocks("super block", get_first_block_offset(vdo), 1);
}

/**********************************************************************/
static void listBlockMap(void)
{
  struct block_map *map = get_block_map(vdo);
  if (map->root_count > 0) {
    listBlocks("block map tree roots", map->root_origin, map->root_count);
  }
}

/**********************************************************************/
static void listSlab(slab_count_t              slab,
                     const struct slab_config *slabConfig)
{
  physical_block_number_t slabOrigin
    = vdo->depot->first_block + (slab * vdo->states.vdo.config.slab_size);

  // List the slab's reference count blocks.
  char buffer[64];
  sprintf(buffer, "slab %u reference blocks", slab);
  listBlocks(buffer, slabOrigin + slabConfig->data_blocks,
             slabConfig->reference_count_blocks);

  // List the slab's journal blocks.
  sprintf(buffer, "slab %u journal", slab);
  listBlocks(buffer, get_slab_journal_start_block(slabConfig, slabOrigin),
             slabConfig->slab_journal_blocks);
}

/**********************************************************************/
static void listSlabs(void)
{
  struct slab_depot        *depot      = vdo->depot;
  slab_count_t              slabCount  = calculate_slab_count(depot);
  const struct slab_config *slabConfig = get_slab_config(depot);
  for (slab_count_t slab = 0; slab < slabCount; slab++) {
    listSlab(slab, slabConfig);
  }
}

/**********************************************************************/
static void listRecoveryJournal(void)
{
  const struct partition *partition
    = get_vdo_partition(vdo->layout, RECOVERY_JOURNAL_PARTITION);
  listBlocks("recovery journal", get_fixed_layout_partition_offset(partition),
             vdo->states.vdo.config.recovery_journal_size);
}

/**********************************************************************/
static void listSlabSummary(void)
{
  const struct partition *partition
    = get_vdo_partition(vdo->layout, SLAB_SUMMARY_PARTITION);
  listBlocks("slab summary", get_fixed_layout_partition_offset(partition),
             get_slab_summary_size(VDO_BLOCK_SIZE));
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

  processArgs(argc, argv);

  // Read input VDO, without validating its config.
  result = readVDOWithoutValidation(vdoBackingName, &vdo);
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
