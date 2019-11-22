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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoReadOnly.c#2 $
 */

#include <err.h>
#include <getopt.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#include "fileUtils.h"
#include "logger.h"

#include "constants.h"
#include "physicalLayer.h"
#include "vdoConfig.h"

#include "fileLayer.h"
#include "vdoVolumeUtils.h"

static const char usageString[] =
  " [--help] filename";

static const char helpString[] =
  "vdoreadonly - Puts a VDO device into read-only mode\n"
  "\n"
  "SYNOPSIS\n"
  "  vdoreadonly filename\n"
  "\n"
  "DESCRIPTION\n"
  "  vdoreadonly forces an existing VDO device into read-only\n"
  "  mode.\n"
  "\n"
  "OPTIONS\n"
  "    --help\n"
  "       Print this help message and exit.\n"
  "\n"
  "    --version\n"
  "       Show the version of vdoreadonly.\n"
  "\n";

// N.B. the option array must be in sync with the option string.
static struct option options[] = {
  { "help",                  no_argument,       NULL, 'h' },
  { "version",               no_argument,       NULL, 'V' },
  { NULL,                    0,                 NULL,  0  },
};
static char optionString[] = "h";

static void usage(const char *progname, const char *usageOptionsString)
{
  errx(1, "Usage: %s%s\n", progname, usageOptionsString);
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  int c;
  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", helpString);
      exit(0);
      break;

    case 'V':
      fprintf(stdout, "vdoreadonly version is: %s\n", CURRENT_VERSION);
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

  openLogger();
  
  char *filename = argv[optind];
  PhysicalLayer *layer;
  int result = makeFileLayer(filename, 0, &layer);
  if (result != VDO_SUCCESS) {
    errx(result, "makeFileLayer failed on '%s'", filename);
  }

  result = setVDOReadOnlyMode(layer);
  if (result != VDO_SUCCESS) {
    char buf[ERRBUF_SIZE];
    errx(result, "setting read-only mode failed on '%s': %s",
         filename, stringError(result, buf, sizeof(buf)));
  }

  // Close and sync the uderlying file.
  layer->destroy(&layer);
}
