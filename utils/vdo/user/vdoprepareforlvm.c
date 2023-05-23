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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoPrepareForLVM.c#9 $
 */

#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "convertToLVM.h"
#include "errors.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "uds.h"

#include "constants.h"
#include "statusCodes.h"
#include "types.h"
#include "vdo.h"
#include "vdoInternal.h"
#include "vdoLoad.h"
#include "vdoState.h"
#include "volumeGeometry.h"

#include "fileLayer.h"

enum {
  MAX_OPEN_RETRIES              = 25,
  OPEN_RETRY_SLEEP_MICROSECONDS = 200000,
};

static const char usageString[] =
  " [--help] [--version] [--check] [--dry-run] filename";

static const char helpString[] =
  "vdoprepareforlvm - Converts a VDO device for use with LVM\n"
  "\n"
  "SYNOPSIS\n"
  "  vdoprepareforlvm [options...] <filename>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdoprepareforlvm converts the VDO block device named by <filename> for\n"
  "  use with LVM. The VDO device to be converted must not be running, and\n"
  "  should not already be an LVM VDO.\n"
  "\n"
  "OPTIONS\n"
  "    --help\n"
  "       Print this help message and exit.\n"
  "\n"
  "    --check\n"
  "       Checks if the specified device has already been converted.\n"
  "\n"
  "    --dry-run\n"
  "       Does alignment calculation but doesn't convert anything.\n"
  "\n"
  "    --version\n"
  "       Show the version of vdoprepareforlvm.\n"
  "\n"
  "EXIT STATUS\n"
  "\n"
  "    -1   Not a VDO block device.\n"
  "\n"
  "     0   Successful conversion/Already converted.\n"
  "\n"
  "     1   Not a converted VDO device (--check only).\n"
  "\n"
  "     2   Error converting/checking the device.\n"
  "\n";

static struct option options[] = {
  { "help",    no_argument, NULL, 'h' },
  { "check",   no_argument, NULL, 'c' },
  { "dry-run", no_argument, NULL, 'd' },
  { "version", no_argument, NULL, 'V' },
  { NULL,      0,           NULL,  0  },
};

static char optionString[] = "cdhV";

static const char *fileName;
static bool checkPreparationState = false;
static bool doDryRun = false;

// Min possible offset we will align geometry block at
static const off_t vdoMinBlockOffset = (1524 * 1024) / VDO_BLOCK_SIZE;

// Max possible offset we will align geomtry block at
static const off_t vdoMaxBlockOffset = (2048 * 1024) / VDO_BLOCK_SIZE;

static void usage(const char *progname, const char *usageOptionsString)
{
  errx(1, "Usage: %s%s\n", progname, usageOptionsString);
}

/**
 * Parse the arguments passed; print command usage if arguments are wrong.
 *
 * @param argc  Number of input arguments
 * @param argv  Array of input arguments
 *
 * @return The backing store of the VDO
 **/
static const char *processArgs(int argc, char *argv[])
{
  int c;
  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    switch (c) {
    case 'c':
      checkPreparationState = true;
      break;

    case 'd':
      doDryRun = true;
      break;

    case 'h':
      printf("%s", helpString);
      exit(0);
      break;

    case 'V':
      printf("%s version is: %s\n", argv[0], CURRENT_VERSION);
      exit(0);
      break;

    default:
      usage(argv[0], usageString);
      break;
    };
  }

  // Explain usage and exit
  if (optind != (argc - 1)) {
    usage(argv[0], usageString);
  }

  return argv[optind++];
}

/**
 * Open the target device with the specified mode.
 * The mode will be bitwise-ored with O_NONBLOCK for the open.
 * On successful open O_NONBLOCK will be cleared.
 *
 * The open will be retried up to MAX_OPEN_RETRIES.
 *
 * @param mode The open mode.
 * @param descriptorPtr  A pointer to hold the opened device file descriptor
 *
 * @return VDO_SUCCES or an error code
 **/
static int openDevice(int mode, int *descriptorPtr)
{
  int fd, result;
  unsigned int retry = 0;

  // Initially attempt to open non-blocking so we can control how long
  // we wait for exclusive access.
  while ((fd = open(fileName, mode | O_NONBLOCK)) < 0) {
    retry++;
    if (retry == 1) {
      printf("Device %s is in use. Retrying...", fileName);
      fflush(stdout);
    } else if (retry > MAX_OPEN_RETRIES) {
      printf("\n");
      return EBUSY;
    } else if ((retry % (1000000 / OPEN_RETRY_SLEEP_MICROSECONDS)) == 0) {
      // Indicate we're still trying about every second.
      printf(".");
      fflush(stdout);
    }

    usleep(OPEN_RETRY_SLEEP_MICROSECONDS);
  }

  // Now that the device is open, unset O_NONBLOCK flag to ensure subsequent
  // I/Os are delayed or blocked correctly.
  result = fcntl(fd, F_SETFL, mode);
  if (result != VDO_SUCCESS) {
    warnx("Unable to clear non-blocking flag for %s", fileName);
    close(fd);
    return result;
  }

  *descriptorPtr = fd;
  return VDO_SUCCESS;
}

/**
 * Open the target device exclusively.
 *
 * @param descriptorPtr  A pointer to hold the opened device file descriptor
 *
 * @return VDO_SUCCES or an error code
 **/
static int openDeviceExclusively(int *descriptorPtr)
{
  return openDevice(O_RDWR | O_EXCL, descriptorPtr);
}

/**
 * Perform the UDS index conversion.
 *
 * @param [in/out] indexConfig       The index configuration to be updated
 * @param [in]     geometry          The volume geometry
 * @param [in]     newBlockOffset    Where the geometry block should move to
 * @param [out]    superblockOffset  A pointer for the superblock byte offset
 *
 * @return UDS_SUCCESS or an error code
 **/
static int convertUDS(IndexConfig    *indexConfig,
                      VolumeGeometry  geometry,
                      off_t           newBlockOffset,
                      off_t          *superblockOffset)
{
  int result;
  UdsConfiguration udsConfig = NULL;
  char *indexName;

  result = indexConfigToUdsConfiguration(indexConfig, &udsConfig);
  if (result != UDS_SUCCESS) {
    warnx("Failed to make UDS configuration for conversion");
    return result;
  }

  udsConfigurationSetNonce(udsConfig, geometry.nonce);

  off_t offset = geometry.regions[INDEX_REGION].startBlock * VDO_BLOCK_SIZE;
  result = asprintf(&indexName, "%s offset=%ld", fileName, offset);
  if (result == -1) {
    udsFreeConfiguration(udsConfig);
    return ENOMEM;
  }

  result = udsConvertToLVM(indexName, newBlockOffset * VDO_BLOCK_SIZE, udsConfig,
                           superblockOffset);
  if (result == UDS_SUCCESS) {
    indexConfig->mem = udsConfigurationGetMemory(udsConfig);
  }

  free(indexName);
  udsFreeConfiguration(udsConfig);

  return result;
}

/**
 * Calculate the aligned offset and extent size for LVM use.
 *
 * In order to properly convert VDO volumes to LVM, they need to
 * be able to fit our max length (256 TB) into two 32 bit numbers;
 * one for extent count and one for extent size. If we take a max
 ^ extent count, it means the min extent size can be 65536. This
 * function attempts to find the max extent size between 1.5 to
 * 2M from the original VDO start location, which we know is free
 * space we can move the start of the VDO volume to after
 * conversion.
 *
 *
 * @param [in]  vdo             Pointer to the VDO structure
 * @param [in]  oldBlockOffset  The offset the geometry block was found at
 * @param [out] extentSize      The extent size for the LVM volume
 * @param [out] newBlockOffset  The offset the geometry block should move to
 *
 * @return VDO_SUCCESS or an error code
 **/
static int calculateAlignment(VDO *vdo,
                              off_t oldBlockOffset,
                              unsigned long *extentSize,
                              off_t *newBlockOffset)
{
  // Length of entire VDO
  unsigned long vdoLength
    = (unsigned long)vdo->config.physicalBlocks * VDO_BLOCK_SIZE;

  // Reset length back to 2MB in from start
  vdoLength -= ((vdoMaxBlockOffset - oldBlockOffset) * VDO_BLOCK_SIZE);

  // Extent range to test with, with a min of 64K needed to handle a
  // 256TB VDO in a 32 bit lvm extent count field
  unsigned long minExtentSize = ((unsigned long)1 << 16);
  unsigned long maxExtentSize = ((unsigned long)1 << 21);

  unsigned long alignRange
    = (vdoMaxBlockOffset - vdoMinBlockOffset) * VDO_BLOCK_SIZE;

  // Find an extent that fits with the length and the area we have to move to
  while (maxExtentSize >= minExtentSize) {
    unsigned long remainder = (vdoLength % maxExtentSize);
    // If we are perfectly aligned at 2M, we're good.
    if (remainder == 0) {
      *extentSize = maxExtentSize;
      *newBlockOffset = vdoMaxBlockOffset;
      return VDO_SUCCESS;
    }
    // If we're not aligned BUT we know we can be aligned
    // within the 500K available space, we'll align there.
    unsigned long neededToAlign = maxExtentSize - remainder;
    if (neededToAlign < alignRange) {
      *extentSize = maxExtentSize;
      *newBlockOffset = vdoMaxBlockOffset - (neededToAlign / VDO_BLOCK_SIZE);
      return VDO_SUCCESS;
    }
    maxExtentSize = maxExtentSize / 2;
  }

  return VDO_NO_SPACE;
}

/**
 * Perform the VDO conversion.
 *
 * @param [in/out] vdo              The vdo structure to be converted
 * @param [in/out] geometry         The volume geometry
 * @param [in]     newBlockOffset   The offset the geometry block should move to
 * @param [in]     indexConfig      The converted index configuration
 * @param [in]     indexStartOffset The converted index start block offset
 *
 * @return VDO_SUCCESS or an error code
 **/
static int convertVDO(VDO            *vdo,
                      VolumeGeometry *geometry,
                      off_t           newBlockOffset,
                      IndexConfig     indexConfig,
                      off_t           indexStartOffset)
{
  int result;
  char *zeroBuf;

  vdo->config.physicalBlocks -= newBlockOffset;
  result = saveVDOComponents(vdo);
  if (result != VDO_SUCCESS) {
    warnx("Failed to save the updated configuration");
    return result;
  }

  geometry->regions[INDEX_REGION].startBlock = indexStartOffset + 1;
  geometry->bioOffset = newBlockOffset;
  geometry->indexConfig = indexConfig;

  PhysicalLayer *offsetLayer;
  result = makeOffsetFileLayer(fileName, 0, newBlockOffset, &offsetLayer);
  if (result != VDO_SUCCESS) {
    warnx("Failed to make offset FileLayer for writing converted volume"
          " geometry");
    return result;
  }

  result = writeVolumeGeometryWithVersion(offsetLayer, geometry, 5);
  offsetLayer->destroy(&offsetLayer);
  if (result != VDO_SUCCESS) {
    warnx("Failed to write the converted volume geometry");
    return result;
  }

  result = vdo->layer->allocateIOBuffer(vdo->layer, VDO_BLOCK_SIZE,
                                        "zero buffer", &zeroBuf);
  if (result != VDO_SUCCESS) {
    warnx("Failed to allocate zero buffer");
    return result;
  }

  result = vdo->layer->writer(vdo->layer, 0, 1, zeroBuf, NULL);
  FREE(zeroBuf);
  if (result != VDO_SUCCESS) {
    warnx("Failed to zero the geometry block from the old VDO location");
    return result;
  }

  return VDO_SUCCESS;
}

/**
 * Clean up and free memory.
 *
 **/
static void cleanup(VDO *vdo, PhysicalLayer *layer)
{
  freeVDO(&vdo);

  if (layer != NULL) {
    layer->destroy(&layer);
  }
}

/**
 * Convert the result from performDeviceCheck() to the appropriate
 * exit status.
 *
 * @param[in] result        The result to convert to an exit status
 * @param[in] preConversion Is the result from pre- or post-conversion VDO?
 *
 * @return exit status      -1 => not a VDO device
 *                           0 => a post-conversion VDO device
 *                           1 => a pre-conversion VDO device
 **/
static int deviceCheckResultToExitStatus(int result, bool preConversion)
{
  int exit_status;

  if ((result != VDO_SUCCESS) && (result != VDO_BAD_MAGIC)) {
    char errBuf[ERRBUF_SIZE];
    errx(2, "Unexpected error accessing '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  if (result == VDO_BAD_MAGIC) {
    exit_status = -1;
  } else if (preConversion) {
    exit_status = 1;
  } else {
    exit_status = 0;
  }

  return exit_status;
}

/**
 * Check if the device has already been converted.
 *
 * @param [out] vdoBlockOffset Block offset where the geometry block is,
 *                             if it is a VDO device.
 *
 * @return exit status      -1 => not a VDO device
 *                           0 => a post-conversion VDO device
 *                           1 => a pre-conversion VDO device
 **/
static int performDeviceCheck(off_t *vdoBlockOffset)
{
  int result;

  // Set default to 0, as that will be true for non converted VDO devices.
  *vdoBlockOffset = 0;

  /**
   * First, check that the device is large enough that we can successfully
   * perform the necessary i/o.  We use 4 MiB as the limit as it is large
   * enough that we can do the i/o but several orders of magnitude smaller
   * than even the smallest possibly configured VDO.
   *
   * This is necessitated by upgrades from pre-9.0 RHEL systems (where this
   * utility is used as part of ensuring all VDO devices have been
   * appropriately converted) to RHEL 9 as systems may have block devices
   * too small to be VDO devices which, for complete safety, couldn't
   * otherwise be excluded from consideration.
   **/
  int fd;
  result = openDevice(O_RDONLY, &fd);
  if (result != VDO_SUCCESS) {
    return deviceCheckResultToExitStatus(result, true);
  }

  uint64_t physicalSize;
  if (ioctl(fd, BLKGETSIZE64, &physicalSize) < 0) {
    close(fd);
    return deviceCheckResultToExitStatus(errno, true);
  }
  close(fd);

  /**
   * If the device is too small to possibly be a VDO use VDO_BAD_MAGIC to
   * generate the "not a VDO" exit status.
   **/
  if (physicalSize < (4 * 1024 * 1024)) {
    return deviceCheckResultToExitStatus(VDO_BAD_MAGIC, true);
  }

  /**
   * The device is large enough that we can perform the necessary i/o to
   * check it.
   **/
  PhysicalLayer *layer;
  result = makeFileLayer(fileName, 0, &layer);
  if (result != VDO_SUCCESS) {
    return result;
  }

  /**
   * If we can load the geometry the device is either an lvm-created VDO, a
   * non-converted VDO or a post-conversion VDO instantiated by lvm.  Anything
   * else would have failed the load with VDO_BAD_MAGIC.
   *
   * In the case of a succesful load we additionally check that the VDO
   * superblock can also be loaded before claiming the device is actually a
   * VDO.
   **/
  VolumeGeometry geometry;
  result = loadVolumeGeometry(layer, &geometry);
  if ((result != VDO_SUCCESS) && (result != VDO_BAD_MAGIC)) {
    cleanup(NULL, layer);
    return deviceCheckResultToExitStatus(result, true);
  }
  if (result == VDO_SUCCESS) {
    // Load the superblock as an additional check the device is really a vdo.
    VDO *vdo;
    result = loadVDOSuperblock(layer, &geometry, false, NULL, &vdo);
    if (result == VDO_SUCCESS) {
      cleanup(vdo, layer);
      return deviceCheckResultToExitStatus(result, true);
    }

    /**
     * The device could be a post-conversion VDO instantiated by lvm.
     * These devices have a geometry from their pre-conversion existence
     * which will cause the loading of the superblock to fail using a default
     * file layer.
     * We try to load the superblock again accounting for the conversion.
     **/
    PhysicalLayer *offsetLayer;
    result = makeOffsetFileLayer(fileName, 0, -vdoMaxBlockOffset, &offsetLayer);
    if (result != VDO_SUCCESS) {
      cleanup(NULL, layer);
      return result;
    }

    result = loadVDOSuperblock(offsetLayer, &geometry, false, NULL, &vdo);
    if (result != VDO_SUCCESS) {
      vdo = NULL;
      result = VDO_BAD_MAGIC; // Not a vdo.
    }

    offsetLayer->destroy(&offsetLayer);
    cleanup(vdo, layer);
    /**
     * Report these devices as pre-conversion in order to present in the
     * same way as lvm-created vdos.
     **/
    return deviceCheckResultToExitStatus(result, true);
  }

  /**
   * Getting here the device is either not a VDO or is a post-conversion VDO
   * specified by the pre-conversion device; i.e., not an lvm instantiated VDO.
   *
   * Attempt to load the geometry from its post-conversion location.  If the
   * device is not a VDO the load will fail with VDO_BAD_MAGIC.
   */
  bool found = false;
  for (off_t block = vdoMaxBlockOffset; block >= vdoMinBlockOffset; block--) {
    result = loadVolumeGeometryAtBlock(layer, block, &geometry);
    if (result == VDO_SUCCESS) {
      printf("Found a geometry block at offset %lu\n",
             (unsigned long)block * VDO_BLOCK_SIZE);
      *vdoBlockOffset = block;
      found = true;
      break;
    }
  }
  if (!found) {
    cleanup(NULL, layer);
    return deviceCheckResultToExitStatus(result, true);
  }

  // Make sure we are already a converted VDO.
  if (geometry.bioOffset == 0) {
    cleanup(NULL, layer);
    return deviceCheckResultToExitStatus(VDO_BAD_CONFIGURATION, true);
  }

  // Load the superblock as an additional check the device is really a vdo.
  VDO *vdo;
  result = loadVDOSuperblock(layer, &geometry, false, NULL, &vdo);
  if (result != VDO_SUCCESS) {
    vdo = NULL;
    result = VDO_BAD_MAGIC; // Not a vdo.
  }

  cleanup(vdo, layer);
  return deviceCheckResultToExitStatus(result, false);
}

/**
 * Perform the device conversion.
 *
 * @param oldBlockOffset  The offset the geometry block was found at.
 *
 * @return UDS_SUCCESS or an error code
 **/
static int performDeviceConversion(off_t oldBlockOffset)
{
  char errBuf[ERRBUF_SIZE];
  int result, fd;
  unsigned long lvmExtentSize = VDO_BLOCK_SIZE;

  printf("Opening %s exclusively\n", fileName);
  result = openDeviceExclusively(&fd);
  if (result != VDO_SUCCESS) {
    errx(2, "Failed to open '%s' exclusively : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  printf("Loading the VDO superblock and volume geometry\n");
  PhysicalLayer *layer;
  result = makeFileLayer(fileName, 0, &layer);
  if (result != VDO_SUCCESS) {
    close(fd);
    errx(2, "Failed to make FileLayer from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  VolumeGeometry geometry;
  result = loadVolumeGeometry(layer, &geometry);
  if (result != VDO_SUCCESS) {
    cleanup(NULL, layer);
    close(fd);
    errx(2, "Failed to load geometry from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  VDO *vdo = NULL;
  result = loadVDOSuperblock(layer, &geometry, false, NULL, &vdo);
  if (result != VDO_SUCCESS) {
    cleanup(vdo, layer);
    close(fd);
    errx(2, "Failed to load superblock from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  off_t newBlockOffset;
  result
    = calculateAlignment(vdo, oldBlockOffset, &lvmExtentSize, &newBlockOffset);
  if (result != VDO_SUCCESS) {
    cleanup(vdo, layer);
    close(fd);
    errx(2, "Failed to calculate alignment from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  printf("Checking the VDO state\n");
  if (vdo->loadState == VDO_NEW) {
    cleanup(vdo, layer);
    close(fd);
    errx(2, "Conversion not recommended for VDO with state NEW.\n"
         "Remove the new VDO and recreate it using LVM.");
  } else if (vdo->loadState != VDO_CLEAN) {
    cleanup(vdo, layer);
    close(fd);
    errx(2, "The VDO is not in a clean state (state '%s' detected).\nPlease"
         " get the volume to a clean state and then re-attempt conversion.",
         getVDOStateName(vdo->loadState));
  }

  // Testing purposes only
  char *vdoOffset = getenv("VDOOFFSET");
  if (vdoOffset != NULL) {
    off_t vdoByteOffset = (off_t)atoi(vdoOffset);
    printf("VDOOFFSET env set. Setting geometry block offset to %lu\n",
           (unsigned long)vdoByteOffset);
    newBlockOffset = vdoByteOffset / VDO_BLOCK_SIZE;
    lvmExtentSize = VDO_BLOCK_SIZE;
  }

  printf("New geometry block offset calculated at %lu\n",
         newBlockOffset * VDO_BLOCK_SIZE);

  // If its not a dry run, convert the VDO device
  if (!doDryRun) {
    IndexConfig config = geometry.indexConfig;
    off_t superblockOffset = 0;

    printf("Converting the UDS index\n");
    result = convertUDS(&config, geometry, newBlockOffset, &superblockOffset);
    if (result != UDS_SUCCESS) {
      cleanup(vdo, layer);
      close(fd);
      errx(2, "Failed to convert the UDS index for usage with LVM: %s",
           stringError(result, errBuf, ERRBUF_SIZE));
    }

    printf("Converting the VDO\n");
    result = convertVDO(vdo,
                        &geometry,
                        newBlockOffset,
                        config,
                        (superblockOffset / VDO_BLOCK_SIZE));
    if (result != VDO_SUCCESS) {
      cleanup(vdo, layer);
      close(fd);
      errx(2, "Failed to convert VDO volume '%s': %s",
           fileName, stringError(result, errBuf, ERRBUF_SIZE));
    }
  }

  cleanup(vdo, layer);
  close(fd);

  printf("Conversion completed for '%s': VDO is now aligned on %ld bytes,"
         " starting at offset %lu\n",
         fileName, lvmExtentSize, newBlockOffset * VDO_BLOCK_SIZE);

  return result;
}

/**
 * Repair the UDS index conversion.
 *
 * @param geometry        The volume geometry
 * @param newBlockOffset  Where the geometry block should move to
 *
 * @return UDS_SUCCESS or an error code
 **/
static int repairUDS(VolumeGeometry geometry, off_t newBlockOffset)
{
  off_t offset = (geometry.regions[INDEX_REGION].startBlock * VDO_BLOCK_SIZE);
  off_t newStartOffset = newBlockOffset * VDO_BLOCK_SIZE;
  return udsRepairConvertToLVM(fileName, offset, newStartOffset);
}

/**
 * Repair the VDO conversion.
 *
 * @param [in/out] vdo              The vdo structure to be converted
 * @param [in/out] geometry         The volume geometry
 * @param [in]     oldBlockOffset   The offset the geometry block was found at
 * @param [in]     newBlockOffset   The offset the geometry block should move to
 *
 * @return VDO_SUCCESS or an error code
 **/
static int repairVDO(VDO            *vdo,
                     VolumeGeometry *geometry,
                     off_t           oldBlockOffset,
                     off_t           newBlockOffset)
{
  int result;
  char *zeroBuf;

  off_t offset = oldBlockOffset - newBlockOffset;
  vdo->config.physicalBlocks += offset;
  result = saveVDOComponents(vdo);
  if (result != VDO_SUCCESS) {
    warnx("Failed to save the updated configuration");
    return result;
  }

  geometry->bioOffset -= offset;

  PhysicalLayer *offsetLayer;
  result = makeOffsetFileLayer(fileName, 0, newBlockOffset, &offsetLayer);
  if (result != VDO_SUCCESS) {
    warnx("Failed to make offset FileLayer for writing converted volume"
          " geometry");
    return result;
  }

  result = writeVolumeGeometryWithVersion(offsetLayer, geometry, 5);
  offsetLayer->destroy(&offsetLayer);
  if (result != VDO_SUCCESS) {
    warnx("Failed to write the converted volume geometry");
    return result;
  }

  result = vdo->layer->allocateIOBuffer(vdo->layer, VDO_BLOCK_SIZE,
                                        "zero buffer", &zeroBuf);
  if (result != VDO_SUCCESS) {
    warnx("Failed to allocate zero buffer");
    return result;
  }

  result = vdo->layer->writer(vdo->layer, oldBlockOffset, 1, zeroBuf, NULL);
  FREE(zeroBuf);
  if (result != VDO_SUCCESS) {
    warnx("Failed to zero the geometry block from the old VDO location");
    return result;
  }

  return VDO_SUCCESS;
}

/**
 * Repair a badly misaligned device conversion.
 *
 * @param oldBlockOffset  The offset the geometry block was found at.
 *
 * @return UDS_SUCCESS or an error code
 **/
static int repairDeviceConversion(off_t oldBlockOffset)
{
  char errBuf[ERRBUF_SIZE];
  int result, fd;

  printf("Opening %s exclusively\n", fileName);
  result = openDeviceExclusively(&fd);
  if (result != VDO_SUCCESS) {
    errx(2, "Failed to open '%s' exclusively : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  printf("Loading the VDO superblock and volume geometry\n");
  PhysicalLayer *layer;
  result = makeFileLayer(fileName, 0, &layer);
  if (result != VDO_SUCCESS) {
    close(fd);
    errx(2, "Failed to make FileLayer from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  VolumeGeometry geometry;
  result = loadVolumeGeometryAtBlock(layer, oldBlockOffset, &geometry);
  if (result != VDO_SUCCESS) {
    cleanup(NULL, layer);
    close(fd);
    errx(2, "Failed to load geometry from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  VDO *vdo = NULL;
  result = loadVDOSuperblock(layer, &geometry, false, NULL, &vdo);
  if (result != VDO_SUCCESS) {
    cleanup(vdo, layer);
    close(fd);
    errx(2, "Failed to load superblock from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  unsigned long lvmExtentSize = VDO_BLOCK_SIZE;
  off_t newBlockOffset;
  result
    = calculateAlignment(vdo, oldBlockOffset, &lvmExtentSize, &newBlockOffset);
  if (result != VDO_SUCCESS) {
    cleanup(vdo, layer);
    close(fd);
    errx(2, "Failed to calculate alignment from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  printf("New geometry block offset calculated at %lu\n",
         newBlockOffset * VDO_BLOCK_SIZE);

  // If there is a change in location and its not a dry run
  if ((newBlockOffset < oldBlockOffset) && !doDryRun) {
    printf("Repairing the UDS index\n");
    result = repairUDS(geometry, newBlockOffset);
    if (result != VDO_SUCCESS) {
      cleanup(vdo, layer);
      close(fd);
      errx(2, "Failed to repair the UDS index for usage with LVM: %s",
           stringError(result, errBuf, ERRBUF_SIZE));
    }

    printf("Repairing the VDO\n");
    result = repairVDO(vdo, &geometry, oldBlockOffset, newBlockOffset);
    if (result != VDO_SUCCESS) {
      cleanup(vdo, layer);
      close(fd);
      errx(2, "Failed to repair VDO volume %s: %s",
           fileName, stringError(result, errBuf, ERRBUF_SIZE));
    }

    result = loadVolumeGeometryAtBlock(layer, newBlockOffset, &geometry);
    if (result != VDO_SUCCESS) {
      cleanup(vdo, layer);
      close(fd);
      errx(2, "Failed to load geometry from '%s' : %s",
           fileName, stringError(result, errBuf, ERRBUF_SIZE));
    }

    result = loadVDOSuperblock(layer, &geometry, false, NULL, &vdo);
    if (result != VDO_SUCCESS) {
      cleanup(vdo, layer);
      close(fd);
      errx(2, "Failed to load superblock from '%s' : %s",
           fileName, stringError(result, errBuf, ERRBUF_SIZE));
    }
  }

  cleanup(vdo, layer);
  close(fd);

  printf("Conversion completed for '%s': VDO is now aligned on %ld bytes,"
         " starting at offset %lu\n",
         fileName, lvmExtentSize, newBlockOffset * VDO_BLOCK_SIZE);

  return result;
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  int exit_status;
  char errBuf[ERRBUF_SIZE];
  off_t vdoBlockOffset = 0;

  exit_status = registerStatusCodes();
  if (exit_status != VDO_SUCCESS) {
    errx(2, "Could not register status codes: %s",
         stringError(exit_status, errBuf, ERRBUF_SIZE));
  }

  fileName = processArgs(argc, argv);
  openLogger();

  printf("Checking device\n");
  exit_status = performDeviceCheck(&vdoBlockOffset);
  if (checkPreparationState) {
    exit(exit_status);
  }

  // -1 not VDO, 0 converted VDO, 1 not converted VDO
  switch (exit_status) {
  case -1:
    errx(exit_status, "'%s' is not a VDO device", fileName);
    break;
  case 0:
    printf("Previously converted VDO. Repairing alignment\n");
    exit_status = repairDeviceConversion(vdoBlockOffset);
    break;
  case 1:
    printf("Non converted VDO. Converting to LVM\n");
    exit_status = performDeviceConversion(vdoBlockOffset);
    break;
  default:
    break;
  }

  exit(exit_status);
}
