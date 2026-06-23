/*
 * Copyright Red Hat
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
 * $Id$
 */

#include <ctype.h>
#include <err.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "status-codes.h"
#include "types.h"
#include "vdoConfig.h"

#include "blockMapUtils.h"
#include "parseUtils.h"
#include "printUtils.h"

#define DEFAULT_BLOCKMAP_CACHE_SIZE 32768
#define DEFAULT_GEOMETRY_BLOCK      1
#define DEFAULT_SUPERBLOCK          1

enum {
  MIN_SLAB_BITS        = 13,
  DEFAULT_SLAB_BITS    = 19,
  MAX_SLAB_BITS        = 23,
};

u64 MIN_VDO_SLAB_SIZE = (1U << 13) * VDO_BLOCK_SIZE;
u64 MAX_VDO_SLAB_SIZE = (1UL << 23) * VDO_BLOCK_SIZE;

#define FIXED_METADATA_BLOCKS DEFAULT_SUPERBLOCK + \
                              DEFAULT_GEOMETRY_BLOCK + \
                              DEFAULT_VDO_BLOCK_MAP_TREE_ROOT_COUNT + \
                              VDO_SLAB_SUMMARY_BLOCKS + \
                              DEFAULT_VDO_RECOVERY_JOURNAL_SIZE

struct vdoInfo {
  /* Input parameters */
  char *indexMemorySize;
  u64 logicalSize;
  u64 physicalSize;
  int sparseIndex;
  unsigned int slabBits;
  u64 slabSize;

  /* UDS info */
  u64 udsIndexSize;
  u64 dedupeWindowSize;

  /* VDO block info */
  u64 physicalBlocks;
  u64 logicalBlocks;
  u64 userDataBlocks;
  int totalSystemBlock;

  /* Slab info */
  int slabSizeInBlock;
  u64 slabCount;
  u64 totalSlabJournal;
  u64 totalReferenceCount;

  /* Block map info */
  u64 blockMapCacheSize;
  u64 totalBlockMapPages;
  u64 totalBlockMapLeaves;
  u64 totalUsableSpace;
  u64 totalForestMemoryUsage;

  int humanReadable;
};

/*
  A VDO volume contains three types of data, system metadata, user metadata
  and user data. All VDO data is stored in a 4K block.

  System metadata are constant regardless the size of VDO volume.  They are:
  Super Block 1
  Geometry Block 1
  Block map root 60
  Slab Summary 64
  Recovery Journal 32K
  UDS index - Created during VDO format.

  The rest of the data are divided into slabs. Within each slab, it contains
  user data and three types of metadata, block map page, reference count
  and slab journal. These meta data usages depend on the size of the VDO and
  the size of each slab.

  Slab size is configurable during VDO format. Default slab size is 2G.
  Slab size can be adjusted using slab bit, default is 19. That is 2 to
  the power of 19 and it is 2G.

  Usable space is calculated by subtracting all the meta data usage from VDO a
  volume.
*/

/*
  Each block map page contains 812 entries, each logical block is one entry.
  First, calculate the total leaves pages by dividing 812. We then calculate
  additional block map pages at the upper level.
*/
static u64 getBlockMapPages(u64 logicalBlocks)
{
  u64 totalBlockMapLeaves = vdo_compute_block_map_page_count(logicalBlocks);
  u64 parentBlockUsage = totalBlockMapLeaves;
  int additionalPage = 0;
  for(int i = 0; i < VDO_BLOCK_MAP_TREE_HEIGHT - 1; i++) {
    parentBlockUsage = DIV_ROUND_UP(parentBlockUsage,
                                    VDO_BLOCK_MAP_ENTRIES_PER_PAGE);
    additionalPage += parentBlockUsage;
  }
  return totalBlockMapLeaves + additionalPage;
}

static u64 getSlabBlockCount(unsigned int slabBits)
{
  u64 blockCount = 1U << slabBits;
  block_count_t numberOfBlocks = (block_count_t) blockCount;
  return numberOfBlocks;
}

static u64 getUDSIndexSize(char *memorySize, int sparseIndex)
{
  int result;
  UdsConfigStrings configStrings;
  char errorBuffer[VDO_MAX_ERROR_MESSAGE_SIZE];

  memset(&configStrings, 0, sizeof(configStrings));
  if (sparseIndex)
    configStrings.sparse = "1";
  configStrings.memorySize = memorySize;

  struct index_config indexConfig;
  result = parseIndexConfig(&configStrings, &indexConfig);
  if (result != VDO_SUCCESS) {
    errx(EXIT_FAILURE, "parseIndexConfig failed: %s",
         uds_string_error(result, errorBuffer, sizeof(errorBuffer)));
  }

  block_count_t indexBlocks = 0;
  result = computeIndexBlocks(&indexConfig, &indexBlocks);
  if (result != VDO_SUCCESS) {
    errx(EXIT_FAILURE, "computeIndexBlocks failed: %s",
         uds_string_error(result, errorBuffer, sizeof(errorBuffer)));
  }
  return indexBlocks;
}

/*
  For deduplication window calculation, please refer to geometry.c in
  utils/uds/geometry.c for detail.
 */
static u64 getDedupeWindowSize(struct vdoInfo *vdoInfo)
{
  char *indexMemorySize = vdoInfo->indexMemorySize;
  u64 dedupeWindowSize;
  if (strcmp(indexMemorySize, "0.25") == 0) {
    dedupeWindowSize = 256;
  } else if ((strcmp(indexMemorySize, "0.5") == 0)
             || (strcmp(indexMemorySize, "0.50") == 0)) {
    dedupeWindowSize = 512;
  } else if (strcmp(indexMemorySize, "0.75") == 0) {
    dedupeWindowSize = 768;
  } else {
    int number;
    int result = parseInt(indexMemorySize, &number);
    if (result != VDO_SUCCESS) {
      errx(EXIT_FAILURE, "parseInt failed: getDedupeWindowSize");
    }
    /* Deduplication window size for dense index is about 1024 times
       size of index memory */
    dedupeWindowSize = (u64)number * 1024;
  }
  /* Return with bytes */
  dedupeWindowSize *= (1024 * 1024 * 1024);
  if (vdoInfo->sparseIndex) {
    /* Deduplication window with sparse index is 10 times larger than
       dense index */
    dedupeWindowSize *= 10;
  }
  return dedupeWindowSize;
}

static void calculateVDOBlockInfo(struct vdoInfo *vdoInfo)
{
  vdoInfo->totalSystemBlock = FIXED_METADATA_BLOCKS + vdoInfo->udsIndexSize;
  vdoInfo->physicalBlocks = vdoInfo->physicalSize / VDO_BLOCK_SIZE;
  vdoInfo->logicalBlocks = vdoInfo->logicalSize / VDO_BLOCK_SIZE;
  vdoInfo->userDataBlocks = vdoInfo->physicalBlocks - vdoInfo->totalSystemBlock;
}

static void calculateSlabInfo(struct vdoInfo *vdoInfo)
{
  vdoInfo->slabSizeInBlock = getSlabBlockCount(vdoInfo->slabBits);
  vdoInfo->slabCount = vdoInfo->userDataBlocks / vdoInfo->slabSizeInBlock;
  vdoInfo->totalSlabJournal = vdoInfo->slabCount * DEFAULT_VDO_SLAB_JOURNAL_SIZE;
  vdoInfo->totalReferenceCount =
    vdo_get_saved_reference_count_size(vdoInfo->userDataBlocks);
}

static void calculateBlockMapMetaInfo(struct vdoInfo *vdoInfo)
{
  vdoInfo->totalBlockMapLeaves =
    vdo_compute_block_map_page_count(vdoInfo->logicalBlocks);
  vdoInfo->totalBlockMapPages = getBlockMapPages(vdoInfo->logicalBlocks);
  vdoInfo->totalForestMemoryUsage =
    vdoInfo->totalBlockMapPages - vdoInfo->totalBlockMapLeaves;
  vdoInfo->totalUsableSpace = (vdoInfo->userDataBlocks -
                               vdoInfo->totalBlockMapPages -
                               vdoInfo->totalReferenceCount -
                               vdoInfo->totalSlabJournal) *
                              VDO_BLOCK_SIZE;
}

static void calculateVDOInfo(struct vdoInfo *vdoInfo)
{
  vdoInfo->dedupeWindowSize = getDedupeWindowSize(vdoInfo);
  vdoInfo->udsIndexSize = getUDSIndexSize(vdoInfo->indexMemorySize,
                                          vdoInfo->sparseIndex);
  calculateVDOBlockInfo(vdoInfo);
  calculateSlabInfo(vdoInfo);
  calculateBlockMapMetaInfo(vdoInfo);
}

static u64 minimumVDOSize(struct vdoInfo *vdoInfo)
{
  /* Minimum VDO size in block equal to minimum fixed layout plus
     index size plus one super block and one geometry block
   */
  return vdoInfo->udsIndexSize + 2 + DEFAULT_VDO_BLOCK_MAP_TREE_ROOT_COUNT +
         DEFAULT_VDO_RECOVERY_JOURNAL_SIZE + vdoInfo->slabSizeInBlock +
         VDO_SLAB_SUMMARY_BLOCKS;
}

static void checkVDOConfigError(struct vdoInfo *vdoInfo)
{
  if (vdoInfo->logicalBlocks > MAXIMUM_VDO_LOGICAL_BLOCKS) {
    errx(EXIT_FAILURE, "Logical size exceeds the maximum: %lu",
         vdoInfo->logicalBlocks);
  }
  if (vdoInfo->physicalBlocks > MAXIMUM_VDO_PHYSICAL_BLOCKS) {
    errx(EXIT_FAILURE, "Physical size exceeds the maximum: %lu",
         vdoInfo->physicalBlocks);
  }
  if (vdoInfo->physicalBlocks < minimumVDOSize(vdoInfo)) {
    errx(EXIT_FAILURE, "Physical size too small");
  }
}

static void printVDOInputParameters(struct vdoInfo *vdoInfo)
{
  char size[PRINTSTRINGSIZE];
  printf("Input parameters:\n");
  getSizeString(vdoInfo->physicalBlocks * VDO_BLOCK_SIZE,
                vdoInfo->humanReadable, size);
  printf("  Physical size: %s\n", size);
  getSizeString(vdoInfo->logicalBlocks * VDO_BLOCK_SIZE,
                vdoInfo->humanReadable, size);
  printf("  Logical size: %s\n", size);
  printf("  Slab bits: %d\n", vdoInfo->slabBits);
  printf("  Sparse: %d\n", vdoInfo->sparseIndex);
  printf("  Index memory: %s\n", vdoInfo->indexMemorySize);
}

static void printVDOStorageUsage(struct vdoInfo *vdoInfo)
{
  char size[PRINTSTRINGSIZE];
  printf("Storage usage:\n");
  getSizeString(vdoInfo->totalUsableSpace,
                vdoInfo->humanReadable, size);
  printf("  Total physical usable size: %s\n", size);
  getSizeString((vdoInfo->totalBlockMapPages * VDO_BLOCK_SIZE),
                 vdoInfo->humanReadable, size);
  printf("  Total block map pages: %s\n", size);
  getSizeString((vdoInfo->udsIndexSize * VDO_BLOCK_SIZE),
                 vdoInfo->humanReadable, size);
  printf("  UDS index size: %s\n", size);
  getSizeString(vdoInfo->dedupeWindowSize,
                vdoInfo->humanReadable, size);
  printf("  Dedupe window: %s\n", size);
  getSizeString((vdo_get_saved_reference_count_size(vdoInfo->userDataBlocks) *
                 VDO_BLOCK_SIZE), vdoInfo->humanReadable, size);
  printf("  Slab reference count usage: %s\n", size);
  getSizeString((vdoInfo->slabCount * DEFAULT_VDO_SLAB_JOURNAL_SIZE *
                 VDO_BLOCK_SIZE), vdoInfo->humanReadable, size);
  printf("  Slab journal usage: %s\n", size);
}

static void printVDOMemoryUsage(struct vdoInfo *vdoInfo)
{
  char size[PRINTSTRINGSIZE];
  printf("VDO in memory usage:\n");
  u64 totalInMemoryUage =
    (vdoInfo->blockMapCacheSize
    + vdoInfo->totalForestMemoryUsage
    + vdo_get_saved_reference_count_size(vdoInfo->userDataBlocks)
    + vdoInfo->udsIndexSize) * VDO_BLOCK_SIZE;
  getSizeString(totalInMemoryUage, vdoInfo->humanReadable, size);
  printf("  Total in memory usage: %s\n", size);
  getSizeString((vdoInfo->blockMapCacheSize * VDO_BLOCK_SIZE),
                vdoInfo->humanReadable, size);
                printf("  Block map cache: %s\n", size);
  getSizeString((vdoInfo->totalForestMemoryUsage * VDO_BLOCK_SIZE),
                vdoInfo->humanReadable, size);
  printf("  Forest memory usage: %s\n", size);
  getSizeString((vdo_get_saved_reference_count_size(vdoInfo->userDataBlocks) *
                 VDO_BLOCK_SIZE), vdoInfo->humanReadable, size);
  printf("  Slab reference count usage: %s\n", size);
  getSizeString((vdoInfo->udsIndexSize * VDO_BLOCK_SIZE),
                vdoInfo->humanReadable, size);
  printf("  UDS index size: %s\n", size);
}

static void printVDOVolumeCharacteristics(struct vdoInfo *vdoInfo)
{
  char size[PRINTSTRINGSIZE];
  printf("Volume characteristics in blocks:\n");
  getSizeString(VDO_BLOCK_SIZE, vdoInfo->humanReadable, size);
  printf("  Blocksize: %s\n", size);
  getSizeString(vdoInfo->physicalBlocks, 0, size);
  printf("  Physical blocks: %s\n", size);
  getSizeString(vdoInfo->logicalBlocks, 0, size);
  printf("  Logical blocks: %s\n", size);
  getSizeString((1U << vdoInfo->slabBits), 0, size);
  printf("  Slab size: %s\n", size);
  printf("  Slab count: %ld\n", vdoInfo->slabCount);
  printf("  Index memory: %s\n", vdoInfo->indexMemorySize);
  printf("  Sparse: %d\n", vdoInfo->sparseIndex);
}

static void printVDOMetaData(struct vdoInfo *vdoInfo)
{
  char size[PRINTSTRINGSIZE];
  printf("VDO metadata:\n");
  getSizeString((DEFAULT_SUPERBLOCK * VDO_BLOCK_SIZE),
                vdoInfo->humanReadable, size);
  printf("  Superblock size: %s\n", size);
  getSizeString((DEFAULT_GEOMETRY_BLOCK * VDO_BLOCK_SIZE),
                vdoInfo->humanReadable, size);
  printf("  Geometry block Size: %s\n", size);
  printf("  VDO block map tree root count: %d\n", DEFAULT_VDO_BLOCK_MAP_TREE_ROOT_COUNT);
  getSizeString((VDO_SLAB_SUMMARY_BLOCKS * VDO_BLOCK_SIZE),
                vdoInfo->humanReadable, size);
  printf("  Slab summary size: %s\n", size);
  getSizeString((DEFAULT_VDO_RECOVERY_JOURNAL_SIZE * VDO_BLOCK_SIZE),
                vdoInfo->humanReadable, size);
  printf("  Recovery journal size: %s\n", size);
  getSizeString((vdoInfo->udsIndexSize * VDO_BLOCK_SIZE),
                vdoInfo->humanReadable, size);;
  printf("  UDS index size: %s\n", size);
  getSizeString((vdoInfo->totalBlockMapPages * VDO_BLOCK_SIZE),
                vdoInfo->humanReadable, size);
  printf("  Total block map pages usage: %s\n", size);
  getSizeString((vdo_get_saved_reference_count_size(vdoInfo->userDataBlocks) *
                 VDO_BLOCK_SIZE),vdoInfo->humanReadable, size);
  printf("  Slab reference count usage: %s\n", size);
  getSizeString((vdoInfo->slabCount * DEFAULT_VDO_SLAB_JOURNAL_SIZE *
                VDO_BLOCK_SIZE), vdoInfo->humanReadable, size);
  printf("  Slab journal usage: %s\n", size);
}

static void printVDOInfo(struct vdoInfo *vdoInfo)
{
  printVDOInputParameters(vdoInfo);
  printVDOStorageUsage(vdoInfo);
  printVDOMemoryUsage(vdoInfo);
  printVDOVolumeCharacteristics(vdoInfo);
  printVDOMetaData(vdoInfo);
}

static int checkSlabSize(u64 slabSize)
{
  char size[PRINTSTRINGSIZE];

  if (slabSize < MIN_VDO_SLAB_SIZE) {
    getSizeString(MIN_VDO_SLAB_SIZE, 1, size);
    warnx("Slab size too small, minimum size %s", size);
    return(VDO_OUT_OF_RANGE);
  }
  if (slabSize > MAX_VDO_SLAB_SIZE) {
    getSizeString(MAX_VDO_SLAB_SIZE, 1, size);
    warnx("Slab size too large, maximum size %s", size);
    return(VDO_OUT_OF_RANGE);
  }
  return VDO_SUCCESS;
}
 
/**********************************************************************/
static void usage(bool printDetail)
{
  fprintf(stderr,
          "Usage: vdoCalculateSize --physical-size=MB\n"
          "                        --logical-size=MB\n"
          "                        [--block-map-cache-size=blocks]\n"
          "                        [--human-readable]\n"
          "                        [--index-memory-size=GB]\n"
          "                        [--slab-bits=bits]\n"
          "                        [--slab-size=MB]\n"
          "                        [--sparse-index]\n"
          "                        [--version]\n\n");
  if (printDetail) {
    fprintf(stderr,
          "DESCRIPTION\n"
          "  Calculate VDO space and memory usage.\n"
          "\n"
          "\n"
          "  --block-map-cache-size=blocks  Size of the block map cache, in 4K blocks\n"
          "\n"
          "  --help                         Display this help and exit\n"
          "\n"
          "  --human-readable               Print sizes in human readable format\n"
          "\n"
          "  --index-memory-size=GB\n"
          "    Specify the amount of memory, in gigabytes, to devote to the\n"
          "    index. Accepted options are .25, .5, .75, and all positive\n"
          "    integers. Default size is 0.25\n"
          "\n"
          "  --logical-size=MB              VDO logical size\n"
          "\n"
          "  --physical-size=MB             VDO physical size\n"
          "\n"
          "  --slab-bits=bits\n"
          "    Set the free space allocator's slab size to 2^<bits> 4 KB blocks.\n"
          "    <bits> must be a value between 4 and 23 (inclusive), corresponding\n"
          "    to a slab size between 128 KB and 32 GB. The default value is 19\n"
          "    which results in a slab size of 2 GB. This allocator manages the\n"
          "    space VDO uses to store user data.\n"
          "    The maximum number of slabs in the system is 8192, so this value\n"
          "    determines the maximum physical size of a VDO volume. One slab is\n"
          "    the minimum amount by which a VDO volume can be grown. Smaller\n"
          "    slabs also increase the potential for parallelism if the device\n"
          "    has multiple physical threads. Therefore, this value should be set\n"
          "    as small as possible, given the eventual maximal size of the\n"
          "    volume.\n"
          "\n"
          "  --slab-size=MB\n"
          "    Set slab size directly instead of using --slab-bits. This\n"
          "    option is mutually exclusive with --slab-bits.\n"
          "\n"
          "  --sparse-index                 Default to false\n"
          "\n"
          "  --version\n                    Output version and exit\n"
          "\n");
  }
  exit(1);
}

static int convertSlabSizeToBit(struct vdoInfo *vdoInfo)
{
  u64 slabBlock = vdoInfo->slabSize / VDO_BLOCK_SIZE;

  if (slabBlock == 0 || (slabBlock & (slabBlock - 1)) != 0) {
    warnx("Slab size is not power of 2");
    return(VDO_OUT_OF_RANGE);
  }
  int slabBits = 0;
  while (slabBlock > 1) {
    slabBlock >>= 1;
    slabBits++;
  }
  vdoInfo->slabBits = slabBits;
  return VDO_SUCCESS;
}

static void parseArgs(int argc, char **argv, struct vdoInfo *vdoInfo)
{
  int result;
  enum { OPT_BLOCKMAP_CACHE_SIZE = 'A', OPT_INDEX_MEMORY_SIZE, OPT_HELP,
         OPT_HUMAN_READABLE, OPT_LOGICAL_SIZE, OPT_PHYSICAL_SIZE, OPT_SLAB_BITS,
         OPT_SLAB_SIZE, OPT_SPARSE_INDEX, OPT_VERSION};
  struct option options[] = {
    {"block-map-cache-size", required_argument, NULL, OPT_BLOCKMAP_CACHE_SIZE},
    {"index-memory-size",    required_argument, NULL, OPT_INDEX_MEMORY_SIZE},
    {"help",                 no_argument,       NULL, OPT_HELP},
    {"human-readable",       no_argument,       NULL, OPT_HUMAN_READABLE},
    {"logical-size",         required_argument, NULL, OPT_LOGICAL_SIZE},
    {"physical-size",        required_argument, NULL, OPT_PHYSICAL_SIZE},
    {"slab-bits",            required_argument, NULL, OPT_SLAB_BITS},
    {"slab-size",            required_argument, NULL, OPT_SLAB_SIZE},
    {"sparse-index",         no_argument,       NULL, OPT_SPARSE_INDEX},
    {"version",              no_argument,       NULL, OPT_VERSION},
    {NULL,                   0,                 NULL, 0},
  };
  int opt;
  bool printDetail = true; 
  while ((opt = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
    switch (opt) {
    case OPT_BLOCKMAP_CACHE_SIZE:
      result = parseSize(optarg, true, &vdoInfo->blockMapCacheSize);
      if (result != VDO_SUCCESS) {
        usage(!printDetail);
      }
      break;
    case OPT_INDEX_MEMORY_SIZE:
      vdoInfo->indexMemorySize = optarg;
      break;
    case OPT_LOGICAL_SIZE:
      result = parseSize(optarg, true, &vdoInfo->logicalSize);
      if (result != VDO_SUCCESS) {
        usage(!printDetail);
      }
      break;
    case OPT_HELP:
      usage(printDetail);
      break;
    case OPT_HUMAN_READABLE:
      vdoInfo->humanReadable = 1;
      break;
    case OPT_PHYSICAL_SIZE:
      result = parseSize(optarg, true, &vdoInfo->physicalSize);
      if (result != VDO_SUCCESS) {
        usage(!printDetail);
      }
      break;
    case OPT_SLAB_BITS:
      result = parseUInt(optarg, MIN_SLAB_BITS, MAX_SLAB_BITS,
                         &(vdoInfo->slabBits));
      if (result != VDO_SUCCESS) {
        warnx("Invalid slab bits, must be %u-%u",
              MIN_SLAB_BITS, MAX_SLAB_BITS);
        usage(!printDetail);
      }
      break;
    case OPT_SLAB_SIZE:
      result = parseSize(optarg, true, &vdoInfo->slabSize);
      if (result != VDO_SUCCESS) {
        usage(!printDetail);
      }
      if (checkSlabSize(vdoInfo->slabSize) != VDO_SUCCESS) {
        usage(!printDetail);
      }
      break;
    case OPT_SPARSE_INDEX:
      vdoInfo->sparseIndex = 1;
      break;
    case OPT_VERSION:
      fprintf(stdout, "vdoCalculation version is: %s\n", CURRENT_VERSION);
      exit(0);
    default:
      usage(printDetail);
      break;
    }
  }
  if (optind < argc) {
    usage(printDetail);
  }
}

static void checkArgs(struct vdoInfo *vdoInfo)
{
  if (vdoInfo->logicalSize == 0 || vdoInfo->physicalSize == 0) {
    warnx("--logical-size and --physical-size are required");
    usage(false);
  }
  if (vdoInfo->slabBits != 0 && vdoInfo->slabSize != 0) {
    warnx("Cannot use --slab-bits and --slab-size together");
    usage(false);
  } else if (vdoInfo->slabBits == 0 && vdoInfo->slabSize == 0) {
    vdoInfo->slabBits = DEFAULT_SLAB_BITS;
  } else if (vdoInfo->slabBits == 0 &&
             convertSlabSizeToBit(vdoInfo) != VDO_SUCCESS) {
    warnx("Problem with --slab-size");
    usage(false);
  }
}

/**********************************************************************/
int main(int argc, char **argv)
{
  struct vdoInfo vdoInfo = {
    .blockMapCacheSize = DEFAULT_BLOCKMAP_CACHE_SIZE,
    .indexMemorySize = "0.25",
    .logicalSize = 0,
    .physicalSize = 0,
    .sparseIndex = 0,
    .slabBits = 0,
    .slabSize = 0, 
    .totalBlockMapLeaves = 0,
    .humanReadable = 0,
  };

  parseArgs(argc, argv, &vdoInfo);
  checkArgs(&vdoInfo);
  calculateVDOInfo(&vdoInfo);
  checkVDOConfigError(&vdoInfo);
  printVDOInfo(&vdoInfo);
  return 0;
}
