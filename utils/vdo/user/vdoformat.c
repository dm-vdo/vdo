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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoFormat.c#8 $
 */

#include <err.h>
#include <getopt.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "uds.h"

#include "errors.h"
#include "fileUtils.h"
#include "logger.h"
#include "stringUtils.h"
#include "syscalls.h"
#include "timeUtils.h"

#include "constants.h"
#include "slabDepot.h"
#include "statusCodes.h"
#include "types.h"
#include "vdo.h"
#include "vdoInternal.h"
#include "vdoConfig.h"
#include "vdoLoad.h"

#include "fileLayer.h"
#include "parseUtils.h"

enum {
  MIN_SLAB_BITS        =  4,
  DEFAULT_SLAB_BITS    = 19,
};

static const char usageString[] =
  " [--help] [options...] filename";

static const char helpString[] =
  "vdoformat - format a VDO device\n"
  "\n"
  "SYNOPSIS\n"
  "  vdoformat [options] filename\n"
  "\n"
  "DESCRIPTION\n"
  "  vdoformat formats the block device named by filename as a VDO device\n"
  "  This is analogous to low-level device formatting. The device will not\n"
  "  be formatted if it already contains a VDO, unless the --force flag is\n"
  "  used.\n"
  "\n"
  "  vdoformat can also modify some of the formatting parameters.\n"
  "\n"
  "OPTIONS\n"
  "    --force\n"
  "       Format the block device, even if there is already a VDO formatted\n"
  "       thereupon.\n"
  "\n"
  "    --help\n"
  "       Print this help message and exit.\n"
  "\n"
  "    --logical-size=<size>\n"
  "       Set the logical (provisioned) size of the VDO device to <size>.\n"
  "       A size suffix of K for kilobytes, M for megabytes, G for\n"
  "       gigabytes, T for terabytes, or P for petabytes is optional. The\n"
  "       default unit is megabytes.\n"
  "\n"
  "    --slab-bits=<bits>\n"
  "      Set the free space allocator's slab size to 2^<bits> 4 KB blocks.\n"
  "      <bits> must be a value between 4 and 23 (inclusive), corresponding\n"
  "      to a slab size between 128 KB and 32 GB. The default value is 19\n"
  "      which results in a slab size of 2 GB. This allocator manages the\n"
  "      space VDO uses to store user data.\n"
  "\n"
  "      The maximum number of slabs in the system is 8192, so this value\n"
  "      determines the maximum physical size of a VDO volume. One slab is\n"
  "      the minimum amount by which a VDO volume can be grown. Smaller\n"
  "      slabs also increase the potential for parallelism if the device\n"
  "      has multiple physical threads. Therefore, this value should be set\n"
  "      as small as possible, given the eventual maximal size of the\n"
  "      volume.\n"
  "\n"
  "    --uds-checkpoint-frequency=<frequency>\n"
  "       Specify the frequency of checkpoints. The default is never.\n"
  "\n"
  "    --uds-memory-size=<gigabytes>\n"
  "       Specify the amount of memory, in gigabytes, to devote to the\n"
  "       index. Accepted options are .25, .5, .75, and all positive\n"
  "       integers.\n"
  "\n"
  "    --uds-sparse\n"
  "       Specify whether or not to use a sparse index.\n"
  "\n"
  "    --verbose\n"
  "       Describe what is being formatted and with what parameters.\n"
  "\n"
  "    --version\n"
  "       Show the version of vdoformat.\n"
  "\n";

// N.B. the option array must be in sync with the option string.
static struct option options[] = {
  { "force",                    no_argument,       NULL, 'f' },
  { "help",                     no_argument,       NULL, 'h' },
  { "logical-size",             required_argument, NULL, 'l' },
  { "slab-bits",                required_argument, NULL, 'S' },
  { "uds-checkpoint-frequency", required_argument, NULL, 'c' },
  { "uds-memory-size",          required_argument, NULL, 'm' },
  { "uds-sparse",               no_argument,       NULL, 's' },
  { "verbose",                  no_argument,       NULL, 'v' },
  { "version",                  no_argument,       NULL, 'V' },
  { NULL,                       0,                 NULL,  0  },
};
static char optionString[] = "fhil:S:c:m:svV";

static void usage(const char *progname, const char *usageOptionsString)
{
  errx(1, "Usage: %s%s\n", progname, usageOptionsString);
}

/**********************************************************************/
static void printReadableSize(size_t size)
{
  const char *UNITS[] = { "B", "KB", "MB", "GB", "TB", "PB" };
  unsigned int unit = 0;
  while ((size >= 1024) && (unit < COUNT_OF(UNITS) - 1)) {
    size /= 1024;
    unit++;
  };
  printf("%zu %s", size, UNITS[unit]);
}

/**********************************************************************/
static void describeCapacity(const VDO    *vdo,
                             uint64_t      logicalSize,
                             unsigned int  slabBits)
{
  if (logicalSize == 0) {
    printf("Logical blocks defaulted to %" PRIu64 " blocks.\n",
           vdo->config.logicalBlocks);
  }

  SlabCount slabCount = calculate_slab_count(vdo->depot);
  const SlabConfig *slabConfig = get_slab_config(vdo->depot);
  size_t totalSize = slabCount * slabConfig->slabBlocks * VDO_BLOCK_SIZE;
  size_t maxTotalSize = MAX_SLABS * slabConfig->slabBlocks * VDO_BLOCK_SIZE;

  printf("The VDO volume can address ");
  printReadableSize(totalSize);
  printf(" in %u data slab%s", slabCount, (slabCount != 1) ? "s" : "");
  if (slabCount > 1) {
    printf(", each ");
    printReadableSize(slabConfig->slabBlocks * VDO_BLOCK_SIZE);
  }
  printf(".\n");

  if (slabCount < MAX_SLABS) {
    printf("It can grow to address at most ");
    printReadableSize(maxTotalSize);
    printf(" of physical storage in %u slabs.\n", MAX_SLABS);
    if (slabBits < MAX_SLAB_BITS) {
      printf("If a larger maximum size might be needed, use bigger slabs.\n");
    }
  } else {
    printf("The volume has the maximum number of slabs and so cannot grow.\n");
    if (slabBits < MAX_SLAB_BITS) {
      printf("Consider using larger slabs to allow the volume to grow.\n");
    }
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

  uint64_t     logicalSize  = 0; // defaults to physicalSize
  unsigned int slabBits     = DEFAULT_SLAB_BITS;

  UdsConfigStrings configStrings;
  memset(&configStrings, 0, sizeof(configStrings));

  int c;
  uint64_t sizeArg;
  static bool verbose = false;
  static bool force   = false;

  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    switch (c) {
    case 'f':
      force = true;
      break;

    case 'h':
      printf("%s", helpString);
      exit(0);
      break;

    case 'l':
      result = parseSize(optarg, true, &sizeArg);
      if (result != VDO_SUCCESS) {
        usage(argv[0], usageString);
      }
      logicalSize = sizeArg;
      break;

    case 'S':
      result = parseUInt(optarg, MIN_SLAB_BITS, MAX_SLAB_BITS, &slabBits);
      if (result != VDO_SUCCESS) {
        warnx("invalid slab bits, must be %u-%u",
              MIN_SLAB_BITS, MAX_SLAB_BITS);
        usage(argv[0], usageString);
      }
      break;

    case 'c':
      configStrings.checkpointFrequency = optarg;
      break;

    case 'm':
      configStrings.memorySize = optarg;
      break;

    case 's':
      configStrings.sparse = "1";
      break;

    case 'v':
      verbose = true;
      break;

    case 'V':
      fprintf(stdout, "vdoformat version is: %s\n", CURRENT_VERSION);
      exit(0);
      break;

    default:
      usage(argv[0], usageString);
      break;
    };
  }

  if (optind != (argc - 1)) {
    usage(argv[0], usageString);
  }

  char *filename = argv[optind];

  struct stat statbuf;
  result = loggingStatMissingOk(filename, &statbuf, "Getting status");
  if (result != UDS_SUCCESS && result != ENOENT) {
    errx(result, "unable to get status of %s", filename);
  }

  if (!S_ISBLK(statbuf.st_mode)) {
    errx(1, "%s must be a block device", filename);
  }

  int fd;
  result = openFile(filename, FU_READ_WRITE, &fd);
  if (result != UDS_SUCCESS) {
    errx(result, "unable to open %s", filename);
  }

  uint64_t physicalSize;
  if (ioctl(fd, BLKGETSIZE64, &physicalSize) < 0) {
    errx(errno, "unable to get size of %s", filename);
  }

  if (physicalSize > MAXIMUM_PHYSICAL_BLOCKS * VDO_BLOCK_SIZE) {
    errx(1, "underlying block device size exceeds the maximum (%" PRIu64 ")",
        MAXIMUM_PHYSICAL_BLOCKS * VDO_BLOCK_SIZE);
  }

  result = closeFile(fd, "cannot close file");
  if (result != UDS_SUCCESS) {
    errx(1, "cannot close %s", filename);
  }

  VDOConfig config = {
    .logicalBlocks       = logicalSize / VDO_BLOCK_SIZE,
    .physicalBlocks      = physicalSize / VDO_BLOCK_SIZE,
    .slabSize            = 1 << slabBits,
    .slabJournalBlocks   = DEFAULT_SLAB_JOURNAL_SIZE,
    .recoveryJournalSize = DEFAULT_RECOVERY_JOURNAL_SIZE,
  };

  if ((config.logicalBlocks * VDO_BLOCK_SIZE) != (BlockCount) logicalSize) {
    errx(1, "logical size must be a multiple of block size %d",
         VDO_BLOCK_SIZE);
  }

  char errorBuffer[ERRBUF_SIZE];
  if (config.logicalBlocks > MAXIMUM_LOGICAL_BLOCKS) {
    errx(VDO_OUT_OF_RANGE,
         "%" PRIu64 " requested logical space exceeds the maximum "
         "(%" PRIu64 "): %s",
         logicalSize, MAXIMUM_LOGICAL_BLOCKS * VDO_BLOCK_SIZE,
         stringError(VDO_OUT_OF_RANGE, errorBuffer, sizeof(errorBuffer)));
  }

  PhysicalLayer *layer;
  result = makeFileLayer(filename, config.physicalBlocks, &layer);
  if (result != VDO_SUCCESS) {
    errx(result, "makeFileLayer failed on '%s'", filename);
  }

  // Check whether there's a VDO on this device already...
  VDO *vdo;
  result = loadVDO(layer, false, NULL, &vdo);
  if (result == VDO_SUCCESS) {
    if (force) {
      warnx("Formatting device already containing a valid VDO.");
    } else {
      errx(EPERM, "Cannot format device already containing a valid VDO!\n"
           "If you are sure you want to format this device again, use the\n"
           "--force option.");
    }
    freeVDO(&vdo);
  }

  struct index_config indexConfig;
  result = parseIndexConfig(&configStrings, &indexConfig);
  if (result != UDS_SUCCESS) {
    errx(result, "parseIndexConfig failed: %s",
         stringError(result, errorBuffer, sizeof(errorBuffer)));
  }

  // Zero out the UDS superblock in case there's already a UDS there.
  char *zeroBuffer;
  result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
                                   "zero buffer", &zeroBuffer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = layer->writer(layer, 1, 1, zeroBuffer, NULL);
  if (result != VDO_SUCCESS) {
    return result;
  }

  if (verbose) {
    if (logicalSize > 0) {
      printf("Formatting '%s' with %" PRIu64 " logical and %" PRIu64
             " physical blocks of %u bytes.\n",
             filename, config.logicalBlocks, config.physicalBlocks,
             VDO_BLOCK_SIZE);
    } else {
      printf("Formatting '%s' with default logical and %" PRIu64
             " physical blocks of %u bytes.\n",
             filename, config.physicalBlocks, VDO_BLOCK_SIZE);
    }
  }

  result = formatVDO(&config, &indexConfig, layer);
  if (result != VDO_SUCCESS) {
    const char *extraHelp = "";
    if (result == VDO_TOO_MANY_SLABS) {
      extraHelp = "\nReduce the device size or increase the slab size";
    }
    errx(result, "formatVDO failed on '%s': %s%s",
         filename,
         stringError(result, errorBuffer, sizeof(errorBuffer)),
         extraHelp);
  }

  result = loadVDO(layer, true, NULL, &vdo);
  if (result != VDO_SUCCESS) {
    errx(result, "unable to verify configuration after formatting '%s'",
         filename);
  }

  // Display default logical size, max capacity, etc.
  describeCapacity(vdo, logicalSize, slabBits);

  freeVDO(&vdo);

  // Close and sync the underlying file.
  layer->destroy(&layer);
}
