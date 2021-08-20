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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoPrepareForLVM.c#1 $
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

static const char usageString[] =
  " [--help] [--version] filename";

static const char helpString[] =
  "vdoprepareforlvm - Converts a VDO device for use with LVM\n"
  "\n"
  "SYNOPSIS\n"
  "  vdoprepareforlvm <filename>\n"
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
  "    --version\n"
  "       Show the version of vdoprepareforlvm.\n"
  "\n";

static struct option options[] = {
  { "help",    no_argument, NULL, 'h' },
  { "version", no_argument, NULL, 'V' },
  { NULL,      0,           NULL,  0  },
};

static char optionString[] = "hV";

static const char *fileName;
static const off_t vdoByteOffset       = (1024 * 1024) * 2;
static const BlockCount vdoBlockOffset = vdoByteOffset / VDO_BLOCK_SIZE;

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
  unsigned int retries = 25;
  unsigned int micro_sec_delay = 200000;

  while (((fd = open(fileName, O_RDWR | O_EXCL | O_NONBLOCK)) < 0)
         && retries--) {
    if (!retries) {
      return EBUSY;
    }

    usleep(micro_sec_delay);
    printf("Retrying in use check for %s.\n", fileName);
  }

  // Now that the device is open, unset O_NONBLOCK flag to prevent subsequent
  // I/Os from not being delayed or blocked correctly
  result = fcntl(fd, F_SETFL, O_RDWR | O_EXCL);
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
 * Clean up and free memory before exiting.
 *
 **/
static void cleanup(VDO *vdo, PhysicalLayer *layer)
{
  freeVDO(&vdo);

  if (layer != NULL) {
    layer->destroy(&layer);
  }
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  char errBuf[ERRBUF_SIZE];
  int result, fd;

  result = registerStatusCodes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         stringError(result, errBuf, ERRBUF_SIZE));
  }

  fileName = processArgs(argc, argv);
  openLogger();

  printf("Opening %s exclusively\n", fileName);
  result = openDeviceExclusively(&fd);
  if (result != VDO_SUCCESS) {
    errx(1, "Failed to open '%s' exclusively : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  printf("Loading the VDO superblock and volume geometry\n");
  PhysicalLayer *layer;
  result = makeFileLayer(fileName, 0, &layer);
  if (result != VDO_SUCCESS) {
    errx(1, "Failed to make FileLayer from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  VolumeGeometry geometry;
  result = loadVolumeGeometry(layer, &geometry);
  if (result != VDO_SUCCESS) {
    cleanup(NULL, layer);
    errx(1, "Failed to load geometry from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  VDO *vdo = NULL;
  result = loadVDOSuperblock(layer, &geometry, false, NULL, &vdo);
  if (result != VDO_SUCCESS) {
    cleanup(vdo, layer);
    errx(1, "Failed to load superblock from '%s' : %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  printf("Checking the VDO state\n");
  if (vdo->loadState == VDO_NEW) {
    cleanup(vdo, layer);
    errx(1, "Conversion not recommended for VDO with state NEW.\n"
         "Remove the new VDO and recreate it using LVM.");
  } else if (vdo->loadState != VDO_CLEAN) {
    cleanup(vdo, layer);
    errx(1, "The VDO is not in a clean state (state '%s' detected).\nPlease"
         " get the volume to a clean state and then re-attempt conversion.",
         getVDOStateName(vdo->loadState));
  }

  printf("Converting the UDS index\n");
  IndexConfig config = geometry.indexConfig;
  off_t superblockOffset = 0;
  
  result = convertUDS(&config, geometry, &superblockOffset);
  if (result != UDS_SUCCESS) {
    cleanup(vdo, layer);
    errx(1, "Failed to convert the UDS index for usage with LVM: %s",
         stringError(result, errBuf, ERRBUF_SIZE));
  }

  printf("Converting the VDO\n");
  result = convertVDO(vdo,
                      &geometry,
                      config,
                      (superblockOffset / VDO_BLOCK_SIZE));
  if (result != VDO_SUCCESS) {
    cleanup(vdo, layer);
    errx(1, "Failed to convert VDO volume '%s': %s",
         fileName, stringError(result, errBuf, ERRBUF_SIZE));
  }

  cleanup(vdo, layer);
  close(fd);

  printf("Conversion completed for '%s': VDO is now offset by %ld bytes\n",
         fileName, vdoByteOffset);
}
