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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoDumpMetadata.c#23 $
 */

#include <err.h>
#include <getopt.h>

#include "errors.h"
#include "fileUtils.h"
#include "memoryAlloc.h"
#include "stringUtils.h"
#include "syscalls.h"

#include "blockMapInternals.h"
#include "fixedLayout.h"
#include "numUtils.h"
#include "physicalLayer.h"
#include "slabDepotInternals.h"
#include "slabSummary.h"
#include "statusCodes.h"
#include "types.h"
#include "vdoInternal.h"
#include "vdoLayout.h"
#include "volumeGeometry.h"

#include "blockMapUtils.h"
#include "fileLayer.h"
#include "vdoVolumeUtils.h"

enum {
  STRIDE_LENGTH = 256,
  MAX_LBNS      = 255,
};

static const char usageString[]
  = "[--help] [--no-block-map] [--lbn=<lbn>] [--version] vdoBacking"
    " outputFile";

static const char helpString[] =
  "vdodumpmetadata - dump the metadata regions from a VDO device\n"
  "\n"
  "SYNOPSIS\n"
  "  vdodumpmetadata [--no-block-map] [--lbn=<lbn>] <vdoBacking>"
  "    <outputFile>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdodumpmetadata dumps the metadata regions of a VDO device to\n"
  "  another file, to enable save and transfer of metadata from\n"
  "  a VDO without transfer of the entire backing store.\n"
  "\n"
  "  vdodumpmetadata will produce a large output file. The expected size is\n"
  "  roughly equal to VDO's metadata size. A rough estimate of the storage\n"
  "  needed is 1.4 GB per TB of logical space.\n"
  "\n"
  "  If the --no-block-map option is used, the output file will be of size\n"
  "  no higher than 130MB + (9 MB per slab).\n"
  "\n"
  "  --lbn implies --no-block-map, and saves the block map page associated\n"
  "  with the specified LBN in the output file. This option may be\n"
  "  specified up to 255 times.\n"
  "\n";

static struct option options[] = {
  { "help",            no_argument,       NULL, 'h' },
  { "lbn",             required_argument, NULL, 'l' },
  { "no-block-map",    no_argument,       NULL, 'b' },
  { "version",         no_argument,       NULL, 'V' },
  { NULL,              0,                 NULL,  0  },
};

static char                    *vdoBacking     = NULL;
static struct vdo              *vdo            = NULL;

static char                    *outputFilename = NULL;
static int                      outputFD       = -1;
static char                    *buffer         = NULL;

static bool                     noBlockMap     = false;
static uint8_t                  lbnCount       = 0;
static physical_block_number_t *lbns           = NULL;

/**
 * Explain how this command-line tool is used.
 *
 * @param progname           Name of this program
 * @param usageOptionString  Multi-line explanation
 **/
static void usage(const char *progname)
{
  errx(1, "Usage: %s %s\n", progname, usageString);
}

/**
 * Release any and all allocated memory.
 **/
static void freeAllocations(void)
{
  freeVDOFromFile(&vdo);
  trySyncAndCloseFile(outputFD);
  FREE(buffer);
  FREE(lbns);
  buffer = NULL;
}

/**
 * Parse the arguments passed; print command usage if arguments are wrong.
 *
 * @param argc  Number of input arguments
 * @param argv  Array of input arguments
 **/
static void processArgs(int argc, char *argv[])
{
  int   c;
  char *optionString = "hbl:V";
  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", helpString);
      exit(0);

    case 'b':
      noBlockMap = true;
      break;

    case 'l':
      // lbnCount is a uint8_t, so we need to check that we don't
      // overflow it by performing this equality check before incrementing.
      if (lbnCount == MAX_LBNS) {
        errx(1, "Cannot specify more than %u LBNs", MAX_LBNS);
      }

      noBlockMap = true;
      int result = parseUint64(optarg, &lbns[lbnCount++]);
      if (result != VDO_SUCCESS) {
        warnx("Cannot parse LBN as a number");
        usage(argv[0]);
      }
      break;

    case 'V':
      printf("%s version is: %s\n", argv[0], CURRENT_VERSION);
      exit(0);

    default:
      usage(argv[0]);
      break;
    }
  }

  // Explain usage and exit
  if (optind != (argc - 2)) {
    usage(argv[0]);
  }

  vdoBacking     = argv[optind++];
  outputFilename = argv[optind++];
}

/**
 * Copy blocks from the VDO backing to the output file.
 *
 * @param startBlock  The block to start at in the VDO backing
 * @param count       How many blocks to copy
 *
 * @return VDO_SUCCESS or an error
 **/
static int copyBlocks(physical_block_number_t startBlock, block_count_t count)
{
  while ((count > 0)) {
    block_count_t blocksToWrite = min_block(STRIDE_LENGTH, count);
    int result = vdo->layer->reader(vdo->layer, startBlock, blocksToWrite,
                                    buffer, NULL);
    if (result != VDO_SUCCESS) {
      return result;
    }
    result = writeBuffer(outputFD, buffer, blocksToWrite * VDO_BLOCK_SIZE);
    if (result != VDO_SUCCESS) {
      return result;
    }
    startBlock += blocksToWrite;
    count      -= blocksToWrite;
  }
  return VDO_SUCCESS;
}

/**
 * Write a zero block to the output file.
 *
 * @return VDO_SUCCESS or an error
 **/
static int zeroBlock(void)
{
  memset(buffer, 0, VDO_BLOCK_SIZE);
  return writeBuffer(outputFD, buffer, VDO_BLOCK_SIZE);
}

/**
 * Copy the referenced page to the output file.
 *
 * Implements MappingExaminer.
 **/
static int copyPage(struct block_map_slot   slot __attribute__((unused)),
                    height_t                height,
                    physical_block_number_t pbn,
                    BlockMappingState       state)
{
  if ((height == 0) || !isValidDataBlock(vdo->depot, pbn)
      || (state == MAPPING_STATE_UNMAPPED)) {
    // Nothing to add to the dump.
    return VDO_SUCCESS;
  }

  int result = copyBlocks(pbn, 1);
  if (result != VDO_SUCCESS) {
    warnx("Could not copy block map page %" PRIu64, pbn);
  }
  return result;
}

/**********************************************************************/
static void dumpGeometryBlock(void)
{
  // Copy the geometry block.
  int result = copyBlocks(0, 1);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not copy super block");
  }
}

/**********************************************************************/
static void dumpSuperBlock(void)
{
  struct volume_geometry geometry;
  int result = load_volume_geometry(vdo->layer, &geometry);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not load geometry");
  }

  // Copy the super block.
  result = copyBlocks(get_data_region_offset(geometry), 1);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not copy super block");
  }
}

/**********************************************************************/
static void dumpBlockMap(void)
{
  if (!noBlockMap) {
    // Copy the block map.
    struct block_map *map = get_block_map(vdo);
    int result = copyBlocks(map->root_origin, map->root_count);
    if (result != VDO_SUCCESS) {
      errx(1, "Could not copy tree root block map pages");
    }

    result = examineBlockMapEntries(vdo, copyPage);
    if (result != VDO_SUCCESS) {
      errx(1, "Could not copy allocated block map pages");
    }
  } else {
    // Copy any specific block map pages requested.
    for (size_t i = 0; i < lbnCount; i++) {
      physical_block_number_t pagePBN;
      int result = findLBNPage(vdo, lbns[i], &pagePBN);
      if (result != VDO_SUCCESS) {
        errx(1, "Could not read block map for LBN %" PRIu64, lbns[i]);
      }

      if (pagePBN == ZERO_BLOCK) {
        result = zeroBlock();
      } else {
        result = copyBlocks(pagePBN, 1);
      }
      if (result != VDO_SUCCESS) {
        errx(1, "Could not copy block map for LBN %" PRIu64, lbns[i]);
      }
    }
  }
}

/**********************************************************************/
static void dumpSlabs(void)
{
  slab_count_t slabCount = calculate_slab_count(vdo->depot);

  // Copy the slab metadata.
  const struct slab_config *slabConfig = get_slab_config(vdo->depot);
  block_count_t journalBlocks  = slabConfig->slab_journal_blocks;
  block_count_t refCountBlocks = slabConfig->reference_count_blocks;
  for (slab_count_t i = 0; i < slabCount; i++) {
    physical_block_number_t slabStart
      = vdo->depot->first_block + (i * vdo->config.slab_size);
    physical_block_number_t origin = slabStart + slabConfig->data_blocks;
    int result = copyBlocks(origin, refCountBlocks + journalBlocks);
    if (result != VDO_SUCCESS) {
      errx(1, "Could not copy slab metadata");
    }
  }
}

/**********************************************************************/
static void dumpRecoveryJournal(void)
{
  // Copy the recovery journal.
  const struct partition *partition
    = get_vdo_partition(vdo->layout, RECOVERY_JOURNAL_PARTITION);
  int result = copyBlocks(get_fixed_layout_partition_offset(partition),
                          vdo->config.recovery_journal_size);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not copy recovery journal");
  }
}

/**********************************************************************/
static void dumpSlabSummary(void)
{
  // Copy the slab summary.
  const struct partition *partition
    = get_vdo_partition(vdo->layout, SLAB_SUMMARY_PARTITION);
  int result = copyBlocks(get_fixed_layout_partition_offset(partition),
                          get_slab_summary_size(VDO_BLOCK_SIZE));
  if (result != VDO_SUCCESS) {
    errx(1, "Could not copy slab summary");
  }
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

  result = ALLOCATE(MAX_LBNS, physical_block_number_t, __func__, &lbns);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %" PRIu64 " bytes",
         sizeof(physical_block_number_t) * MAX_LBNS);
  }

  processArgs(argc, argv);

  // Read input VDO.
  result = makeVDOFromFile(vdoBacking, true, &vdo);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not load VDO from '%s'", vdoBacking);
  }

  // Allocate buffer for copies.
  size_t copyBufferBytes = STRIDE_LENGTH * VDO_BLOCK_SIZE;
  result = vdo->layer->allocateIOBuffer(vdo->layer, copyBufferBytes,
                                        "copy buffer", &buffer);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not allocate %zu bytes", copyBufferBytes);
  }

  // Open the dump output file.
  result = openFile(outputFilename, FU_CREATE_WRITE_ONLY, &outputFD);
  if (result != UDS_SUCCESS) {
    errx(1, "Could not open output file '%s'", outputFilename);
  }

  dumpGeometryBlock();
  dumpSuperBlock();
  dumpBlockMap();
  dumpSlabs();
  dumpRecoveryJournal();
  dumpSlabSummary();

  freeAllocations();
  exit(0);
}
