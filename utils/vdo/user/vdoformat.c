/*
 * Copyright (c) 2017 Red Hat, Inc.
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
 * $Id: //eng/vdo-releases/magnesium/src/c++/vdo/user/vdoFormat.c#1 $
 */

#include <err.h>
#include <getopt.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "fileUtils.h"
#include "logger.h"
#include "stringUtils.h"
#include "syscalls.h"
#include "timeUtils.h"

#include "constants.h"
#include "types.h"
#include "vdo.h"
#include "vdoConfig.h"

#include "fileLayer.h"
#include "parseUtils.h"

enum {
  MIN_SLAB_BITS        =  4,
  DEFAULT_SLAB_BITS    = 19,
};

typedef enum {
  BLOCK_DEVICE = 1,
  NEW_FILE,
  EXISTING_FILE,
} StorageType;

typedef struct {
  char *sparse;
  char *memorySize;
  char *checkpointFrequency;
} IndexConfig;

static const char usageString[] =
  " [--help] [options...] filename";

static const char helpString[] =
  "vdoformat - format a VDO device\n"
  "\n"
  "SYNOPSIS\n"
  "  vdoformat [options] filename\n"
  "\n"
  "DESCRIPTION\n"
  "  vdoformat formats the file or block device named by filename as a\n"
  "  VDO device.  This is analogous to low-level device formatting.\n"
  "\n"
  "  vdoformat can also modify some of the formatting parameters.\n"
  "\n"
  "OPTIONS\n"
  "    --help\n"
  "       Print this help message and exit.\n"
  "\n"
  "    --index-blocks\n"
  "       The size of the dedupe index, in blocks.\n"
  "\n"
  "    --logical-size=<size>\n"
  "       Set the logical (provisioned) size of the VDO device to <size>.\n"
  "       A size suffix of K for kilobytes, M for megabytes, G for\n"
  "       gigabytes, T for terabytes, or P for petabytes is optional. The\n"
  "       default unit is megabytes.\n"
  "\n"
  "    --physical-size=<size>\n"
  "       Set the physical size of the VDO device to <size>. A size suffix\n"
  "       of K for kilobytes, M for megabytes, G for gigabytes, or T for\n"
  "       terabytes is optional. The default unit is megabytes.\n"
  "\n"
  "    --slab-bits=<bits>\n"
  "       Specify the slab size in bits. The maximum size is 23, and the\n"
  "       default is 19.\n"
  "\n"
  "    --uds-checkpoint-frequency=<frequency>\n"
  "       Specify the frequency of checkpoints.\n"
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
  { "help",                     no_argument,       NULL, 'h' },
  { "index-blocks",             required_argument, NULL, 'n' },
  { "logical-size",             required_argument, NULL, 'l' },
  { "physical-size",            required_argument, NULL, 'p' },
  { "slab-bits",                required_argument, NULL, 'S' },
  { "uds-checkpoint-frequency", required_argument, NULL, 'c' },
  { "uds-memory-size",          required_argument, NULL, 'm' },
  { "uds-sparse",               no_argument,       NULL, 's' },
  { "verbose",                  no_argument,       NULL, 'v' },
  { "version",                  no_argument,       NULL, 'V' },
  { NULL,                       0,                 NULL,  0  },
};
static char optionString[] = "hn:il:p:S:c:m:svV";

static void usage(const char *progname, const char *usageOptionsString)
{
  errx(1, "Usage: %s%s\n", progname, usageOptionsString);
}

/**
 * Read an integer from a file of the given name.
 *
 * @param [in]  filename  The file to read
 * @param [out] intPtr    The uint64_t to read into
 *
 * @return VDO_SUCCESS or an error
 **/
static int readIntFromFile(char *filename, uint64_t *intPtr)
{
  FILE *file = fopen(filename, "r");
  int scanned = fscanf(file, "%" PRIu64, intPtr);
  if (scanned != 1) {
    return UDS_SHORT_READ;
  }
  fclose(file);
  return VDO_SUCCESS;
}

/**
 * Write a buffer into the file of the given name.
 *
 * @param [in] filename  The file to write
 * @param [in] buffer    The buffer to write from
 *
 * @return VDO_SUCCESS or an error
 **/
static int writeFile(char *filename, char *buffer)
{
  int fd;
  int result = openFile(filename, FU_CREATE_WRITE_ONLY, &fd);
  if (result != VDO_SUCCESS) {
    return logErrorWithStringError(result, "File open on %s failed",
                                   filename);
  }

  size_t size = strnlen(buffer, VDO_BLOCK_SIZE);
  result = writeBuffer(fd, buffer, size);
  if (result != VDO_SUCCESS) {
    return logErrorWithStringError(result, "File write on %s failed",
                                   filename);
  }

  return closeFile(fd, "File close failed");
}

/**
 *  Set a configuration sysfs node.
 *
 * @param what        The reason to allocate a sysfs node name string
 * @param deviceName  The name of the UDS device
 * @param param       The name of the configuration parameter
 * @param value       The value to set the configuration parameter to
 *
 * @return VDO_SUCCESS or an error
 **/
static int setConfigurationSysfsNode(const char *what,
                                     char       *deviceName,
                                     char       *param,
                                     char       *value)
{
  char *sysfsFilename;
  int result = allocSprintf(what, &sysfsFilename,
                            "/sys/uds/configuration/%s/%s", deviceName, param);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = writeFile(sysfsFilename, value);
  if (result != VDO_SUCCESS) {
    return result;
  }
  return VDO_SUCCESS;
}

/**
 * Use an extant UDS device config to format appropriately.
 *
 * @param [in]  config          The UDS config
 * @param [in]  filename        The actual name of the device being formatted
 * @param [in]  tempDeviceName  The temporary name of the UDS configuration
 * @param [out] indexBytesPtr   A pointer to return the size of the index
 *
 * @return VDO_SUCCESS or an error
 **/
static int doFormat(IndexConfig  *config,
                    char         *filename,
                    char         *tempDeviceName,
                    uint64_t     *indexBytesPtr)
{
  int result = VDO_SUCCESS;
  if (config->checkpointFrequency != NULL) {
    result = setConfigurationSysfsNode("checkpoint frequency sysfs node",
                                       tempDeviceName, "checkpoint_frequency",
                                       config->checkpointFrequency);
    if (result != VDO_SUCCESS) {
      return result;
    }
  }

  if (config->memorySize != NULL) {
    result = setConfigurationSysfsNode("memory size sysfs node",
                                       tempDeviceName, "mem",
                                       config->memorySize);
    if (result != VDO_SUCCESS) {
      return result;
    }
  }

  if (config->sparse != NULL) {
    result = setConfigurationSysfsNode("sparse sysfs node", tempDeviceName,
                                       "sparse", config->sparse);
    if (result != VDO_SUCCESS) {
      return result;
    }
  }

  char *configString;
  result = allocSprintf("UDS configuration string", &configString,
                        "dev=%s offset=4096", filename);
  if (result != VDO_SUCCESS) {
    return result;
  }

  // Do the formatting.
  result = setConfigurationSysfsNode("UDS formatting sysfs node",
                                     tempDeviceName, "create_index",
                                     configString);

  if (result != VDO_SUCCESS) {
    return result;
  }

  result = writeFile("/sys/uds/close_index", configString);
  if (result != VDO_SUCCESS) {
    return result;
  }

  char *sizeFilename;
  result = allocSprintf("size sysfs node", &sizeFilename,
                        "/sys/uds/configuration/%s/size", tempDeviceName);
  if (result != VDO_SUCCESS) {
    return result;
  }

  return readIntFromFile(sizeFilename, indexBytesPtr);
}

/**
 * Format the device for UDS.
 *
 * @param [in]  config          The UDS config
 * @param [in]  filename        The name of the device being formatted
 * @param [out] indexBytesPtr   A pointer to return the size of the index
 *
 * @return VDO_SUCCESS or an error
 **/
static int formatUDS(IndexConfig *config,
                     char        *filename,
                     uint64_t    *indexBytesPtr)
{
  char *tempDeviceName;
  int result = allocSprintf("temporary UDS device name", &tempDeviceName,
                            "%" PRIu64, nowUsec());
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = writeFile("/sys/uds/create_config", tempDeviceName);
  if (result != VDO_SUCCESS) {
    return result;
  }

  result = doFormat(config, filename, tempDeviceName, indexBytesPtr);
  // In case of error, we still need to delete the config.

  int result2 = writeFile("/sys/uds/delete_config", tempDeviceName);
  if (result != VDO_SUCCESS) {
    return result;
  }

  return result2;
}

int main(int argc, char *argv[])
{
  uint64_t     indexBlocks  = 0;
  uint64_t     logicalSize  = 0; // defaults to physicalSize
  uint64_t     physicalSize = 0;
  unsigned int slabBits     = DEFAULT_SLAB_BITS;

  IndexConfig udsConfig;
  memset(&udsConfig, 0, sizeof(udsConfig));

  int c;
  uint64_t sizeArg;
  int result;
  static bool verbose = false;

  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", helpString);
      exit(0);
      break;

    case 'n':
      // XXX If uds-memory-size is the only way to pass UDS size, this should
      // go away.
      result = parseSize(optarg, true, &sizeArg);
      if (result != VDO_SUCCESS) {
        usage(argv[0], usageString);
      }
      indexBlocks = sizeArg;
      break;

    case 'l':
      result = parseSize(optarg, true, &sizeArg);
      if (result != VDO_SUCCESS) {
        usage(argv[0], usageString);
      }
      logicalSize = sizeArg;
      break;

    case 'p':
      result = parseSize(optarg, true, &sizeArg);
      if ((result != VDO_SUCCESS) || (sizeArg == 0)) {
        usage(argv[0], usageString);
      }
      physicalSize = sizeArg;
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
      udsConfig.checkpointFrequency = optarg;
      break;

    case 'm':
      udsConfig.memorySize = optarg;
      break;

    case 's':
      udsConfig.sparse = "1";
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

  openLogger();

  StorageType storageType;
  struct stat statbuf;
  result = loggingStatMissingOk(filename, &statbuf, "Getting status");
  if (result != UDS_SUCCESS && result != ENOENT) {
    errx(result, "unable to get status of %s", filename);
  }

  if (result == ENOENT) {
    storageType = NEW_FILE;
  } else if (S_ISREG(statbuf.st_mode)) {
    storageType = EXISTING_FILE;
  } else if (S_ISBLK(statbuf.st_mode)) {
    storageType = BLOCK_DEVICE;
  } else {
    errx(1, "%s must be a file or block device", filename);
  }

  if (storageType == NEW_FILE) {
    if (physicalSize == 0) {
      errx(1, "missing required physical size");
    }
    if (logicalSize == 0) {
      logicalSize = physicalSize;
    }
  }

  int fd;
  if (storageType == NEW_FILE) {
    result = openFile(filename, FU_CREATE_READ_WRITE, &fd);
  } else {
    result = openFile(filename, FU_READ_WRITE, &fd);
  }
  if (result != UDS_SUCCESS) {
    errx(result, "unable to open %s", filename);
  }

  if (storageType == BLOCK_DEVICE) {
    uint64_t bytes;
    if (ioctl(fd, BLKGETSIZE64, &bytes) < 0) {
      errx(errno, "unable to get size of %s", filename);
    }
    if (physicalSize == 0) {
      physicalSize = bytes;
    } else if (physicalSize != bytes) {
      errx(1, "physical size must be the size of the underlying block device");
    }
    if (logicalSize == 0) {
      logicalSize = physicalSize;
    }
  } else {
    if (storageType == EXISTING_FILE) {
      if (physicalSize == 0) {
        physicalSize = statbuf.st_size;
      } else {
        if (physicalSize != (uint64_t) statbuf.st_size) {
          errx(1, "physical size must be the size of the underlying file");
        }
      }
      if (logicalSize == 0) {
        logicalSize = physicalSize;
      }
    }
    if (storageType == NEW_FILE) {
      if (ftruncate(fd, physicalSize) != 0) {
        errx(errno, "unable to extend %s to %ld bytes",
             filename, physicalSize);
      }
    }
  }
  result = closeFile(fd, "cannot close file");
  if (result != UDS_SUCCESS) {
    errx(1, "cannot close %s", filename);
  }

  BlockCount physicalBlocks = physicalSize / VDO_BLOCK_SIZE;

  if ((physicalBlocks * VDO_BLOCK_SIZE) != (BlockCount) physicalSize) {
    errx(1, "physical size must be a multiple of block size %d",
         VDO_BLOCK_SIZE);
  }

  if (logicalSize == 0) {
    logicalSize = physicalSize;
  }

  BlockCount logicalBlocks = logicalSize / VDO_BLOCK_SIZE;
  if ((logicalBlocks * VDO_BLOCK_SIZE) != (BlockCount) logicalSize) {
    errx(1, "logical size must be a multiple of block size %d",
         VDO_BLOCK_SIZE);
  }

  char errorBuffer[ERRBUF_SIZE];
  if (logicalBlocks > MAXIMUM_LOGICAL_BLOCKS) {
    errx(VDO_OUT_OF_RANGE,
         "%" PRIu64 " requested logical space exceeds the maximum "
         "(%" PRIu64 "): %s",
         logicalSize, MAXIMUM_LOGICAL_BLOCKS * VDO_BLOCK_SIZE,
         stringError(VDO_OUT_OF_RANGE, errorBuffer, sizeof(errorBuffer)));
  }

  PhysicalLayer *layer;
  result = makeFileLayer(filename, physicalBlocks, &layer);
  if (result != VDO_SUCCESS) {
    errx(result, "makeFileLayer failed on '%s'", filename);
  }

  uint64_t indexBytes;
  result = formatUDS(&udsConfig, filename, &indexBytes);
  if (result != VDO_SUCCESS) {
    errx(result, "formatUDS failed on '%s': %s",
         filename, stringError(result, errorBuffer, sizeof(errorBuffer)));
  }

  indexBlocks = indexBytes / VDO_BLOCK_SIZE;
  if ((indexBlocks * VDO_BLOCK_SIZE) != (BlockCount) indexBytes) {
    errx(1, "index size must be a multiple of block size %d",
         VDO_BLOCK_SIZE);
  }

  VDOConfig config = {
    .logicalBlocks       = logicalBlocks,
    .physicalBlocks      = physicalBlocks,
    .slabSize            = 1 << slabBits,
    .slabJournalBlocks   = DEFAULT_SLAB_JOURNAL_SIZE,
    .recoveryJournalSize = DEFAULT_RECOVERY_JOURNAL_SIZE,
  };

  if (verbose) {
    printf("Formatting '%s' with %" PRIu64 " logical and %" PRIu64
           " physical blocks of %" PRIu64 " bytes.\n",
           filename, logicalBlocks, physicalBlocks, (uint64_t) VDO_BLOCK_SIZE);
  }

  result = formatVDO(&config, indexBlocks, layer);
  if (result != VDO_SUCCESS) {
    errx(result, "formatVDO failed on '%s': %s",
         filename, stringError(result, errorBuffer, sizeof(errorBuffer)));
  }

  // Close and sync the underlying file.
  layer->destroy(&layer);
}
