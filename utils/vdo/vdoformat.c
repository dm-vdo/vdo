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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoFormat.c#30 $
 */

#include <blkid/blkid.h>
#include <dirent.h>
#include <err.h>
#include <getopt.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "uds.h"

#include "errors.h"
#include "fileUtils.h"
#include "logger.h"
#include "stringUtils.h"
#include "syscalls.h"
#include "timeUtils.h"

#include "constants.h"
#include "statusCodes.h"
#include "types.h"
#include "vdoConfig.h"

#include "fileLayer.h"
#include "parseUtils.h"
#include "userVDO.h"
#include "vdoVolumeUtils.h"

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
static void describeCapacity(const UserVDO *vdo,
                             uint64_t       logicalSize,
                             unsigned int   slabBits)
{
  if (logicalSize == 0) {
    printf("Logical blocks defaulted to %" PRIu64 " blocks.\n",
           vdo->states.vdo.config.logical_blocks);
  }

  struct slab_config slabConfig = vdo->states.slab_depot.slab_config;
  size_t totalSize = vdo->slabCount * slabConfig.slab_blocks * VDO_BLOCK_SIZE;
  size_t maxTotalSize = MAX_VDO_SLABS * slabConfig.slab_blocks
                          * VDO_BLOCK_SIZE;

  printf("The VDO volume can address ");
  printReadableSize(totalSize);
  printf(" in %u data slab%s", vdo->slabCount,
         ((vdo->slabCount != 1) ? "s" : ""));
  if (vdo->slabCount > 1) {
    printf(", each ");
    printReadableSize(slabConfig.slab_blocks * VDO_BLOCK_SIZE);
  }
  printf(".\n");

  if (vdo->slabCount < MAX_VDO_SLABS) {
    printf("It can grow to address at most ");
    printReadableSize(maxTotalSize);
    printf(" of physical storage in %u slabs.\n", MAX_VDO_SLABS);
    if (slabBits < MAX_VDO_SLAB_BITS) {
      printf("If a larger maximum size might be needed, use bigger slabs.\n");
    }
  } else {
    printf("The volume has the maximum number of slabs and so cannot grow.\n");
    if (slabBits < MAX_VDO_SLAB_BITS) {
      printf("Consider using larger slabs to allow the volume to grow.\n");
    }
  }
}

static const char MSG_FAILED_SIG_OFFSET[] = "Failed to get offset of the %s" \
  " signature on %s.\n";
static const char MSG_FAILED_SIG_LENGTH[] = "Failed to get length of the %s" \
  " signature on %s.\n";
static const char MSG_FAILED_SIG_INVALID[] = "Found invalid data in the %s" \
  " signature on %s.\n";
static const char MSG_SIG_DATA[] = "Found existing signature on %s at" \
  " offset %s: LABEL=\"%s\" UUID=\"%s\" TYPE=\"%s\" USAGE=\"%s\".\n";

/**********************************************************************
 * Print info on existing signature found by blkid. If called with
 * force, print messages to stdout, otherwise print messages to stderr
 *
 * @param probe    the current blkid probe location
 * @param filename the name of the file blkid is probing
 * @param force    whether we called vdoformat with --force.
 *
 * @return VDO_SUCCESS or error.
 */
static int printSignatureInfo(blkid_probe probe,
		  	      const char *filename,
			      bool force)
{
  const char *offset = NULL, *type = NULL, *magic = NULL,
    *usage = NULL, *label = NULL, *uuid = NULL;
  size_t len;

  int result = VDO_SUCCESS;
  result = blkid_probe_lookup_value(probe, "TYPE", &type, NULL);
  if (result == VDO_SUCCESS) {
    result = blkid_probe_lookup_value(probe, "SBMAGIC_OFFSET", &offset, NULL);
    if (result != VDO_SUCCESS) {
      fprintf(force ? stdout : stderr, MSG_FAILED_SIG_OFFSET, type, filename);
    }

    result = blkid_probe_lookup_value(probe, "SBMAGIC", &magic, &len);
    if (result != VDO_SUCCESS) {
      fprintf(force ? stdout : stderr, MSG_FAILED_SIG_LENGTH, type, filename);
    }
  } else {
    result = blkid_probe_lookup_value(probe, "PTTYPE", &type, NULL);
    if (result != VDO_SUCCESS) {
      // Unknown type. Ingore.
      return VDO_SUCCESS;
    }

    result = blkid_probe_lookup_value(probe, "PTMAGIC_OFFSET", &offset, NULL);
    if (result != VDO_SUCCESS) {
      fprintf(force ? stdout: stderr, MSG_FAILED_SIG_OFFSET, type, filename);
    }

    result = blkid_probe_lookup_value(probe, "PTMAGIC", &magic, &len);
    if (result != VDO_SUCCESS) {
      fprintf(force ? stdout : stderr, MSG_FAILED_SIG_LENGTH, type, filename);
    }
    usage = "partition table";
  }

  if ((len == 0) || (offset == NULL)) {
    fprintf(force ? stdout : stderr, MSG_FAILED_SIG_INVALID, type, filename);
  }

  if (usage == NULL) {
    (void) blkid_probe_lookup_value(probe, "USAGE", &usage, NULL);
  }

  /* Return values ignored here, in the worst case we print NULL */
  (void) blkid_probe_lookup_value(probe, "LABEL", &label, NULL);
  (void) blkid_probe_lookup_value(probe, "UUID", &uuid, NULL);

  fprintf(force ? stdout : stderr, MSG_SIG_DATA, filename, offset, label,
	  uuid, type, usage);

  return VDO_SUCCESS;
}

/**********************************************************************
 * Check for existing signatures on disk using blkid.
 *
 * @param filename the name of the file blkid is probing
 * @param force    whether we called vdoformat with --force.
 *
 * @return VDO_SUCCESS or error.
 */
static int checkForSignaturesUsingBlkid(const char *filename, bool force)
{
  int result = VDO_SUCCESS;

  blkid_probe probe = NULL;
  probe = blkid_new_probe_from_filename(filename);
  if (probe == NULL) {
    errx(EIO, "Failed to create a new blkid probe for device %s", filename);
  }

  blkid_probe_enable_partitions(probe, 1);
  blkid_probe_set_partitions_flags(probe, BLKID_PARTS_MAGIC);

  blkid_probe_enable_superblocks(probe, 1);
  blkid_probe_set_superblocks_flags(probe, BLKID_SUBLKS_LABEL |
				    BLKID_SUBLKS_UUID |
				    BLKID_SUBLKS_TYPE |
				    BLKID_SUBLKS_USAGE |
				    BLKID_SUBLKS_VERSION |
				    BLKID_SUBLKS_MAGIC |
				    BLKID_SUBLKS_BADCSUM);

  struct buffer *buffer = NULL;
  result = make_buffer(0, &buffer);
  if (result != VDO_SUCCESS) {
    blkid_free_probe(probe);
    return ENOMEM;
  }

  int found = 0;
  while (blkid_do_probe(probe) == VDO_SUCCESS) {
    found++;
    printSignatureInfo(probe, filename, force);
  }

  if (found > 0) {
    if (force) {
      printf("Formatting device already containing a known signature.\n");
    } else {
      fprintf(stderr, "Cannot format device already containing a known signature!\n"
           "If you are sure you want to format this device again, use the\n"
           "--force option.\n");
      result = EPERM;
    }
  }

  free(buffer);
  blkid_free_probe(probe);

  return result;
}


/**********************************************************************
 * Count the number of processes holding access to the device
 *
 * @param path   the path to the holders sysfs directory.
 * @param force  pointer to holder count variable.
 *
 * @return VDO_SUCCESS or error.
 */
static int countHolders(char *path, int *holders)
{
  struct stat statbuf;
  int result = logging_stat(path, &statbuf, "Getting holder count");
  if (result != UDS_SUCCESS) {
    fprintf(stderr, "Unable to get status of %s.\n", path);
    return result;
  }

  struct dirent *dirent;
  DIR *d = opendir(path);
  if (d == NULL) {
    fprintf(stderr, "Unable to open holders directory.\n");
    return EPERM;
  }

  while ((dirent = readdir(d))) {
    if (strcmp(dirent->d_name, ".") && strcmp(dirent->d_name, "..")) {
      (*holders)++;
    }
  }
  closedir(d);

  return VDO_SUCCESS;
}

#define HOLDER_CHECK_RETRIES 25
#define HOLDER_CHECK_USLEEP_DELAY 200000

/**********************************************************************
 * Check that the device we are about to format is not in use by
 * something else.
 *
 * @param filename the name of the device we are checking
 * @param major    the device's major number
 * @param minor    the device's minor number
 *
 * @return VDO_SUCCESS or error.
 */
static int checkDeviceInUse(char *filename, uint32_t major, uint32_t minor)
{
  unsigned int retries = HOLDER_CHECK_RETRIES;
  int holders = 0;

  char *path;
  int result = alloc_sprintf(__func__, &path, "/sys/dev/block/%" PRIu32
			    ":%" PRIu32 "/holders", major, minor);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = countHolders(path, &holders);
  if (result != VDO_SUCCESS) {
    free(path);
    return result;
  }

  while (holders > 0 && retries--) {
    if (!retries) {
      fprintf(stderr, "The device %s is in use.\n", filename);
      free(path);
      return EPERM;
    }

    usleep(HOLDER_CHECK_USLEEP_DELAY);
    printf("Retrying in use check for %s.\n", filename);
    int result = countHolders(path, &holders);
    if (result != VDO_SUCCESS) {
      free(path);
      return result;
    }
  }
  return VDO_SUCCESS;
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  static char errBuf[ERRBUF_SIZE];

  int result = register_status_codes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         uds_string_error(result, errBuf, ERRBUF_SIZE));
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
      result = parseUInt(optarg, MIN_SLAB_BITS, MAX_VDO_SLAB_BITS, &slabBits);
      if (result != VDO_SUCCESS) {
        warnx("invalid slab bits, must be %u-%u",
              MIN_SLAB_BITS, MAX_VDO_SLAB_BITS);
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
  result = logging_stat_missing_ok(filename, &statbuf, "Getting status");
  if (result != UDS_SUCCESS && result != ENOENT) {
    errx(result, "unable to get status of %s", filename);
  }

  if (!S_ISBLK(statbuf.st_mode)) {
    errx(1, "%s must be a block device", filename);
  }

  uint32_t major = major(statbuf.st_rdev);
  uint32_t minor = minor(statbuf.st_rdev);

  result = checkDeviceInUse(filename, major, minor);
  if (result != VDO_SUCCESS) {
    errx(result, "checkDeviceInUse failed on %s", filename);
  }

  int fd;
  result = open_file(filename, FU_READ_WRITE, &fd);
  if (result != UDS_SUCCESS) {
    errx(result, "unable to open %s", filename);
  }

  uint64_t physicalSize;
  if (ioctl(fd, BLKGETSIZE64, &physicalSize) < 0) {
    errx(errno, "unable to get size of %s", filename);
  }

  if (physicalSize > MAXIMUM_VDO_PHYSICAL_BLOCKS * VDO_BLOCK_SIZE) {
    errx(1, "underlying block device size exceeds the maximum (%" PRIu64 ")",
        MAXIMUM_VDO_PHYSICAL_BLOCKS * VDO_BLOCK_SIZE);
  }

  result = close_file(fd, "cannot close file");
  if (result != UDS_SUCCESS) {
    errx(1, "cannot close %s", filename);
  }

  struct vdo_config config = {
    .logical_blocks        = logicalSize / VDO_BLOCK_SIZE,
    .physical_blocks       = physicalSize / VDO_BLOCK_SIZE,
    .slab_size             = 1 << slabBits,
    .slab_journal_blocks   = DEFAULT_VDO_SLAB_JOURNAL_SIZE,
    .recovery_journal_size = DEFAULT_VDO_RECOVERY_JOURNAL_SIZE,
  };

  if ((config.logical_blocks * VDO_BLOCK_SIZE) != (block_count_t) logicalSize) {
    errx(1, "logical size must be a multiple of block size %d",
         VDO_BLOCK_SIZE);
  }

  char errorBuffer[ERRBUF_SIZE];
  if (config.logical_blocks > MAXIMUM_VDO_LOGICAL_BLOCKS) {
    errx(VDO_OUT_OF_RANGE,
         "%" PRIu64 " requested logical space exceeds the maximum "
         "(%" PRIu64 "): %s",
         logicalSize, MAXIMUM_VDO_LOGICAL_BLOCKS * VDO_BLOCK_SIZE,
         uds_string_error(VDO_OUT_OF_RANGE, errorBuffer, sizeof(errorBuffer)));
  }

  PhysicalLayer *layer;
  result = makeFileLayer(filename, config.physical_blocks, &layer);
  if (result != VDO_SUCCESS) {
    errx(result, "makeFileLayer failed on '%s'", filename);
  }

  // Check whether there's already something on this device already...
  result = checkForSignaturesUsingBlkid(filename, force);
  if (result != VDO_SUCCESS) {
    errx(result, "checkForSignaturesUsingBlkid failed on '%s'", filename);
  }

  struct index_config indexConfig;
  result = parseIndexConfig(&configStrings, &indexConfig);
  if (result != UDS_SUCCESS) {
    errx(result, "parseIndexConfig failed: %s",
         uds_string_error(result, errorBuffer, sizeof(errorBuffer)));
  }

  // Zero out the UDS superblock in case there's already a UDS there.
  char *zeroBuffer;
  result = layer->allocateIOBuffer(layer, VDO_BLOCK_SIZE,
                                   "zero buffer", &zeroBuffer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = layer->writer(layer, 1, 1, zeroBuffer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  if (verbose) {
    if (logicalSize > 0) {
      printf("Formatting '%s' with %" PRIu64 " logical and %" PRIu64
             " physical blocks of %u bytes.\n",
             filename, config.logical_blocks, config.physical_blocks,
             VDO_BLOCK_SIZE);
    } else {
      printf("Formatting '%s' with default logical and %" PRIu64
             " physical blocks of %u bytes.\n",
             filename, config.physical_blocks, VDO_BLOCK_SIZE);
    }
  }

  result = formatVDO(&config, &indexConfig, layer);
  if (result != VDO_SUCCESS) {
    const char *extraHelp = "";
    if (result == VDO_TOO_MANY_SLABS) {
      extraHelp = "\nReduce the device size or increase the slab size";
    }
    if (result == VDO_NO_SPACE) {
      block_count_t minVDOBlocks = 0;
      int calcResult = calculateMinimumVDOFromConfig(&config,
						     &indexConfig,
						     &minVDOBlocks);
      if (calcResult != VDO_SUCCESS) {
	errx(calcResult,
	     "Unable to calculate minimum required VDO size");
      } else {
	uint64_t minimumSize = minVDOBlocks * VDO_BLOCK_SIZE;
	fprintf(stderr,
		"Minimum required size for VDO volume: %" PRIu64 " bytes\n",
		minimumSize);
      }
    }
    errx(result, "formatVDO failed on '%s': %s%s",
         filename,
         uds_string_error(result, errorBuffer, sizeof(errorBuffer)),
         extraHelp);
  }

  UserVDO *vdo;
  result = loadVDO(layer, true, &vdo);
  if (result != VDO_SUCCESS) {
    errx(result, "unable to verify configuration after formatting '%s'",
         filename);
  }

  // Display default logical size, max capacity, etc.
  describeCapacity(vdo, logicalSize, slabBits);

  freeUserVDO(&vdo);

  // Close and sync the underlying file.
  layer->destroy(&layer);
}
