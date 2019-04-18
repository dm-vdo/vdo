/*
 * Copyright (c) 2018 Red Hat, Inc.
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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoDumpConfig.c#2 $
 */

#include <err.h>
#include <getopt.h>
#include <uuid/uuid.h>
#include <stdio.h>

#include "logger.h"
#include "memoryAlloc.h"

#include "constants.h"
#include "types.h"
#include "vdoInternal.h"
#include "volumeGeometry.h"

#include "vdoVolumeUtils.h"

static const char usageString[] = "[--help] vdoBacking";

static const char helpString[] =
  "vdodumpconfig - dump the configuration of a VDO volume from its backing\n"
  "                store.\n"
  "\n"
  "SYNOPSIS\n"
  "  vdodumpconfig <vdoBacking>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdodumpconfig dumps the configuration of a VDO volume, whether or not\n"
  "  the VDO is running.\n"
  "\n";

static struct option options[] = {
  { "help",            no_argument,       NULL, 'h' },
  { NULL,              0,                 NULL,  0  },
};

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
 * Parse the arguments passed; print command usage if arguments are wrong.
 *
 * @param argc  Number of input arguments
 * @param argv  Array of input arguments
 *
 * @return The backing store of the VDO
 **/
static const char *processArgs(int argc, char *argv[])
{
  int   c;
  char *optionString = "h";
  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", helpString);
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

  return argv[optind++];
}

/**********************************************************************/
static void readVDOConfig(const char *vdoBacking,
                          VDOConfig  *configPtr,
                          UUID       *uuidPtr)
{
  VDO *vdo;
  int result = makeVDOFromFile(vdoBacking, true, &vdo);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not load VDO from '%s'", vdoBacking);
  }

  *configPtr = vdo->config;

  VolumeGeometry geometry;
  result = loadVolumeGeometry(vdo->layer, &geometry);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not read VDO geometry from '%s'", vdoBacking);
  }
  memcpy(*uuidPtr, geometry.uuid, sizeof(UUID));

  freeVDOFromFile(&vdo);
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  static char errBuf[ERRBUF_SIZE];

  int result = registerStatusCodes();
  if (result != UDS_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         stringError(result, errBuf, ERRBUF_SIZE));
  }

  const char *vdoBacking = processArgs(argc, argv);

  openLogger();

  VDOConfig config;
  UUID      rawUuid;
  readVDOConfig(vdoBacking, &config, &rawUuid);

  char prettyUuid[37];
  uuid_unparse(rawUuid, prettyUuid);

  printf("VDOConfig:\n");
  printf("  blockSize: %d\n", VDO_BLOCK_SIZE);
  printf("  logicalBlocks: %" PRIu64 "\n", config.logicalBlocks);
  printf("  physicalBlocks: %" PRIu64 "\n", config.physicalBlocks);
  printf("  slabSize: %" PRIu64 "\n", config.slabSize);
  printf("  recoveryJournalSize: %" PRIu64 "\n", config.recoveryJournalSize);
  printf("  slabJournalBlocks: %" PRIu64 "\n", config.slabJournalBlocks);
  printf("UUID: %s\n", prettyUuid);
  exit(0);
}
