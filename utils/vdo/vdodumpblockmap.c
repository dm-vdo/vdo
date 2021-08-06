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
 * $Id: //eng/vdo-releases/sulfur-rhel9.0-beta/src/c++/vdo/user/vdoDumpBlockMap.c#1 $
 */

#include <err.h>
#include <getopt.h>
#include <stdio.h>

#include "errors.h"
#include "logger.h"

#include "blockMapFormat.h"
#include "statusCodes.h"
#include "types.h"

#include "blockMapUtils.h"
#include "vdoVolumeUtils.h"

static const char usageString[]
  = "[--help] [--lba=<lba>] [--version] <filename>";

static const char helpString[] =
  "vdoDumpBlockMap - dump the LBA->PBA mappings of a VDO device\n"
  "\n"
  "SYNOPSIS\n"
  "  vdoDumpBlockMap [--lba=<lba>] <filename>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdoDumpBlockMap dumps all (or only the specified) LBA->PBA mappings\n"
  "  from a cleanly shut down VDO device\n";

static struct option options[] = {
  { "help",       no_argument,       NULL, 'h' },
  { "lba",        required_argument, NULL, 'l' },
  { "version",    no_argument,       NULL, 'V' },
  { NULL,         0,                 NULL,  0  },
};

static logical_block_number_t lbn = 0xFFFFFFFFFFFFFFFF;

static UserVDO *vdo;

/**
 * Explain how this command-line function is used.
 *
 * @param progname           Name of this program
 * @param usageOptionString  Multi-line explanation
 **/
static void usage(const char *progname, const char *usageOptionsString)
{
  fprintf(stderr, "Usage: %s %s\n", progname, usageOptionsString);
  exit(1);
}

/**
 * Get the filename (or "help") from the input arguments.
 * Print command usage if arguments are wrong.
 *
 * @param [in]  argc       Number of input arguments
 * @param [in]  argv       Array of input arguments
 * @param [out] filename   Name of this VDO's file or block device
 *
 * @return VDO_SUCCESS or some error.
 **/
static int processDumpArgs(int argc, char *argv[], char **filename)
{
  int      c;
  char    *optionString = "l:hV";
  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    if (c == (int) 'h') {
      printf("%s", helpString);
      exit(0);
    }

    if (c == (int) 'V') {
      printf("%s version is: %s\n", argv[0], CURRENT_VERSION);
      exit(0);
    }

    if (c == (int) 'l') {
      char *endptr;
      errno = 0;
      lbn = strtoull(optarg, &endptr, 0);
      if (errno == ERANGE || errno == EINVAL || endptr == optarg) {
        errx(1, "No LBA specified");
      }
    }
  }

  // Explain usage and exit
  if (optind != (argc - 1)) {
    usage(argv[0], usageString);
  }

  *filename = argv[optind];

  return VDO_SUCCESS;
}

/**********************************************************************/
static int dumpLBN(void)
{
  physical_block_number_t  pbn;
  enum block_mapping_state state;
  int result = findLBNMapping(vdo, lbn, &pbn, &state);
  if (result != VDO_SUCCESS) {
    warnx("Could not read mapping for lbn %llu", (unsigned long long) lbn);
    return result;
  }

  printf("%llu\t", (unsigned long long) lbn);
  switch (state) {
  case VDO_MAPPING_STATE_UNMAPPED:
    printf("unmapped   \t%llu\n", (unsigned long long) pbn);
    break;

  case VDO_MAPPING_STATE_UNCOMPRESSED:
    printf("mapped     \t%llu\n", (unsigned long long) pbn);
    break;

  default:
    printf("compressed \t%llu slot %u\n",
           (unsigned long long) pbn, vdo_get_slot_from_state(state));
    break;
  }

  return VDO_SUCCESS;
}

/**
 * Print out a mapping from a block map page.
 *
 * Implements MappingExaminer.
 **/
static int dumpBlockMapEntry(struct block_map_slot    slot,
                             height_t                 height,
                             physical_block_number_t  pbn,
                             enum block_mapping_state state)
{
  if ((state != VDO_MAPPING_STATE_UNMAPPED) || (pbn != VDO_ZERO_BLOCK)) {
    printf("PBN %llu\t slot %u\t height %u\t"
           "-> PBN %llu (compression state %u)\n",
           (unsigned long long) slot.pbn, slot.slot, height,
           (unsigned long long) pbn, state);
  }
  return VDO_SUCCESS;
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  static char errBuf[ERRBUF_SIZE];

  int result = register_vdo_status_codes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         uds_string_error(result, errBuf, ERRBUF_SIZE));
  }

  char *filename;
  result = processDumpArgs(argc, argv, &filename);
  if (result != VDO_SUCCESS) {
    exit(1);
  }

  result = makeVDOFromFile(filename, true, &vdo);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not load VDO from '%s': %s",
         filename, uds_string_error(result, errBuf, ERRBUF_SIZE));
  }

  result = ((lbn != 0xFFFFFFFFFFFFFFFF)
            ? dumpLBN() : examineBlockMapEntries(vdo, dumpBlockMapEntry));
  freeVDOFromFile(&vdo);
  exit((result == VDO_SUCCESS) ? 0 : 1);
}
