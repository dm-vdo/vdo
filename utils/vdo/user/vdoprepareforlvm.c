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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoPrepareForLVM.c#5 $
 */

#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
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
  " [--help] [--version] [--check] filename";

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
  { "version", no_argument, NULL, 'V' },
  { NULL,      0,           NULL,  0  },
};

static char optionString[] = "chV";

static const char *fileName;
static bool checkPreparationState = false;
static const off_t vdoByteOffset  = (1024 * 1024) * 2;
static const off_t vdoBlockOffset = vdoByteOffset / VDO_BLOCK_SIZE;

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
 * Open the target device exclusively.
 *
 * @param descriptorPtr  A pointer to hold the opened device file descriptor
 *
 * @return VDO_SUCCES or an error code
 **/
static int openDeviceExclusively(int *descriptorPtr)
{
  int fd, result;
  unsigned int retry = 0;
  int modeFlags = O_RDWR | O_EXCL;

  // Initially attempt to open non-blocking so we can control how long
  // we wait for exclusive access.
  while ((fd = open(fileName, modeFlags | O_NONBLOCK)) < 0) {
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
  result = fcntl(fd, F_SETFL, modeFlags);
  if (result != VDO_SUCCESS) {
    warnx("Unable to clear non-blocking flag for %s", fileName);
    close(fd);
    return result;
  }

  *descriptorPtr = fd;
  return VDO_SUCCESS;
}

/**
 * Perform the UDS index conversion.
 *
 * @param [in/out] indexConfig       The index configuration to be updated
 * @param [in]     geometry          The volume geometry
 * @param [out]    superblockOffset  A pointer for the superblock byte offset
 *
 * @return UDS_SUCCESS or an error code
 **/
static int convertUDS(IndexConfig    *indexConfig,
                      VolumeGeometry  geometry,
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

  off_t startByte = geometry.regions[INDEX_REGION].startBlock * VDO_BLOCK_SIZE;
  result = asprintf(&indexName, "%s offset=%ld", fileName, startByte);
  if (result == -1) {
    udsFreeConfiguration(udsConfig);
    return ENOMEM;
  }

  result = udsConvertToLVM(indexName, vdoByteOffset, udsConfig,
                           superblockOffset);
  if (result == UDS_SUCCESS) {
    indexConfig->mem = udsConfigurationGetMemory(udsConfig);
  }

  free(indexName);
  udsFreeConfiguration(udsConfig);

  return result;
}

/**
 * Perform the VDO conversion.
 *
 * @param [in/out] vdo               The vdo structure to be converted
 * @param [in/out] geometry          The volume geometry
 * @param [in]     indexConfig       The converted index configuration
 * @param [in]     indexStartOffset  The converted index start block offset
 *
 * @return VDO_SUCCESS or an error code
 **/
static int convertVDO(VDO            *vdo,
                      VolumeGeometry *geometry,
                      IndexConfig     indexConfig,
                      off_t           indexStartOffset)
{
  int result;
  char *zeroBuf;

  vdo->config.physicalBlocks -= vdoBlockOffset;
  result = saveVDOComponents(vdo);
  if (result != VDO_SUCCESS) {
    warnx("Failed to save the updated configuration");
    return result;
  }

  geometry->regions[INDEX_REGION].startBlock = indexStartOffset + 1;
  geometry->bioOffset = vdoBlockOffset;
  geometry->indexConfig = indexConfig;

  PhysicalLayer *offsetLayer;
  result = makeOffsetFileLayer(fileName, 0, vdoBlockOffset, &offsetLayer);
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
 * @return exit status      -1 => not a VDO device
 *                           0 => a post-conversion VDO device
 *                           1 => a pre-conversion VDO device
 **/
static int performDeviceCheck(void)
{
  int result;

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
    result = makeOffsetFileLayer(fileName, 0, -vdoBlockOffset, &offsetLayer);
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
  result = loadVolumeGeometryAtBlock(layer, vdoBlockOffset, &geometry);
  if (result != VDO_SUCCESS) {
    cleanup(NULL, layer);
    return deviceCheckResultToExitStatus(result, false);
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
 **/
static int performDeviceConversion(void)
{
  char errBuf[ERRBUF_SIZE];
  int result, fd;

  printf("Opening %s exclusively\n", fileName);
  result = openDeviceExclusively(&fd);
  if (result != VDO_SUCCESS) {
    errx(2, "Failed to open '%s' exclusively : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  int exit_status = performDeviceCheck();
  if ((exit_status == 0) || (exit_status == -1)) {
    close(fd);
    if (exit_status == 0) {
      errx(exit_status, "'%s' is already converted", fileName);
    }
    errx(exit_status, "'%s' is not a VDO device", fileName);
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

  printf("Converting the UDS index\n");
  IndexConfig config = geometry.indexConfig;
  off_t superblockOffset = 0;

  result = convertUDS(&config, geometry, &superblockOffset);
  if (result != UDS_SUCCESS) {
    cleanup(vdo, layer);
    close(fd);
    errx(2, "Failed to convert the UDS index for usage with LVM: %s",
         stringError(result, errBuf, ERRBUF_SIZE));
  }

  printf("Converting the VDO\n");
  result = convertVDO(vdo,
                      &geometry,
                      config,
                      (superblockOffset / VDO_BLOCK_SIZE));
  if (result != VDO_SUCCESS) {
    cleanup(vdo, layer);
    close(fd);
    errx(2, "Failed to convert VDO volume '%s': %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  cleanup(vdo, layer);
  close(fd);

  printf("Conversion completed for '%s': VDO is now offset by %ld bytes\n",
         fileName, vdoByteOffset);

  return result;
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  int exit_status;
  char errBuf[ERRBUF_SIZE];

  exit_status = registerStatusCodes();
  if (exit_status != VDO_SUCCESS) {
    errx(2, "Could not register status codes: %s",
         stringError(exit_status, errBuf, ERRBUF_SIZE));
  }

  fileName = processArgs(argc, argv);
  openLogger();

  if (checkPreparationState) {
    exit_status = performDeviceCheck();
  } else {
    exit_status = performDeviceConversion();
  }

  exit(exit_status);
}
