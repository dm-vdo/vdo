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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoSetUUID.c#1 $
 */

#include <err.h>
#include <getopt.h>
#include <uuid/uuid.h>
#include <stdio.h>

#include "errors.h"
#include "logger.h"
#include "memoryAlloc.h"

#include "constants.h"
#include "types.h"
#include "statusCodes.h"
#include "vdoInternal.h"
#include "vdoVolumeUtils.h"
#include "volumeGeometry.h"

static const char usageString[] = " [options...] vdoBacking";

static const char helpString[] =
  "vdosetuuid - sets a new uuid for the vdo volume stored on a backing\n"
  "             store.\n"
  "\n"
  "SYNOPSIS\n"
  "  vdosetuuid [options] <vdoBacking>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdosetuuid sets a new uuid for the VDO volume stored on the\n"
  "  backing store, whether or not the VDO is running.\n"
  "\n"
  "OPTIONS\n"
  "    --help\n"
  "       Print this help message and exit.\n"
  "\n"
  "    --uuid=<uuid>\n"
  "      Sets the uuid value that is stored in the VDO device. If not\n"
  "      specified, the uuid is randomly generated.\n"
  "\n"
  "    --version\n"
  "       Show the version of the tool.\n"
  "\n";

// N.B. the option array must be in sync with the option string.
static struct option options[] = {
  { "help",                     no_argument,       NULL, 'h' },
  { "uuid",                     required_argument, NULL, 'u' },
  { "version",                  no_argument,       NULL, 'V' },
  { NULL,                       0,                 NULL,  0  },
};
static char optionString[] = "h:u";

static void usage(const char *progname, const char *usageOptionsString)
{
  errx(1, "Usage: %s%s\n", progname, usageOptionsString);
}

uuid_t uuid;

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
  int c, result;
  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", helpString);
      exit(0);

    case 'u':
      result = uuid_parse(optarg, uuid);
      if (result != VDO_SUCCESS) {
        usage(argv[0], usageString);
      }
      break;

    case 'V':
      fprintf(stdout, "vdosetuuid version is: %s\n", CURRENT_VERSION);
      exit(0);
      break;

    default:
      usage(argv[0], usageString);
      break;
    }
  }

  // Explain usage and exit
  if (optind != (argc - 1)) {
    usage(argv[0], usageString);
  }

  return argv[optind++];
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  STATIC_ASSERT(sizeof(uuid_t) == sizeof(UUID));

  static char errBuf[ERRBUF_SIZE];

  int result = registerStatusCodes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
	 stringError(result, errBuf, ERRBUF_SIZE));
  }

  // Generate a UUID as a default value in case the options is not specified.
  uuid_generate(uuid);

  const char *vdoBacking = processArgs(argc, argv);

  openLogger();

  VDO *vdo;
  result = makeVDOFromFile(vdoBacking, false, &vdo);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not load VDO from '%s'", vdoBacking);
  }

  VolumeGeometry geometry;
  result = loadVolumeGeometry(vdo->layer, &geometry);
  if (result != VDO_SUCCESS) {
    freeVDOFromFile(&vdo);
    errx(1, "Could not load the geometry from '%s'", vdoBacking);
  }

  memcpy(geometry.uuid, uuid, sizeof(UUID));

  result = writeVolumeGeometry(vdo->layer, &geometry);
  if (result != VDO_SUCCESS) {
    freeVDOFromFile(&vdo);
    errx(1, "Could not write the geometry to '%s' %s", vdoBacking,
	 stringError(result, errBuf, ERRBUF_SIZE));
  }

  freeVDOFromFile(&vdo);

  exit(0);
}
