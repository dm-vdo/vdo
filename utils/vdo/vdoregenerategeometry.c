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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoRegenerateGeometry.c#16 $
 */

#include <err.h>
#include <getopt.h>
#include <uuid/uuid.h>

#include "memoryAlloc.h"
#include "uds.h"
#include "timeUtils.h"

#include "constants.h"
#include "blockMapFormat.h"
#include "blockMapPage.h"
#include "statusCodes.h"
#include "volumeGeometry.h"

#include "fileLayer.h"
#include "parseUtils.h"
#include "userVDO.h"
#include "vdoVolumeUtils.h"

static const char usageString[]
  = "[--help] [--version] [--offset <offset>] <filename>";

static const char helpString[] =
  "vdoRegenerateGeometry - regenerate a VDO whose first few blocks have been wiped\n"
  "\n"
  "SYNOPSIS\n"
  "  vdoRegenerateGeometry [--offset <offset>] <filename>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdoRegenerateGeometry will attempt to regenerate the geometry block of a\n"
  "  VDO device in the event that the beginning of the backing store was wiped.\n"
  "  This tool will fail if enough of the device was wiped that the VDO super\n"
  "  block was also erased, or if there are multiple valid super block\n"
  "  candidates on the volume.\n"
  "\n"
  "  If the super block location is known, or to select one of the candidate\n"
  "  super blocks in the event that multiple candidates were found, the\n"
  "  --offset option can be used to specify the location (in bytes) of the\n"
  "  super block on the backing store.\n"
  "\n";

enum {
  // This should use UDS_MEMORY_CONFIG_MAX instead of the explicit 1024, but
  // the compiler won't let us.
  UDS_CONFIGURATIONS = (1024 + 3) * 2,
};

static struct option options[] = {
  { "help",    no_argument,       NULL, 'h' },
  { "version", no_argument,       NULL, 'V' },
  { "offset",  required_argument, NULL, 'o' },
  { NULL,      0,                 NULL,  0  },
};

typedef struct {
  char                    memoryString[8];
  bool                    sparse;
  struct volume_geometry  geometry;
  UserVDO                *vdo;
} Candidate;

static char          *blockBuffer;
static PhysicalLayer *fileLayer;
static block_count_t  physicalSize;
static uuid_t         uuid;
static char           errorBuffer[ERRBUF_SIZE];
static Candidate      candidates[UDS_CONFIGURATIONS];
static int            candidateCount = 0;

static char   *fileName = NULL;
static size_t  offset   = 0;

/**
 * Explain how this command-line tool is used.
 *
 * @param programName  Name of this program
 * @param usageString  Multi-line explanation
 **/
static void usage(const char *programName)
{
  errx(1, "\n  Usage: %s %s\n", programName, usageString);
}

/**
 * Parse the arguments passed; print command usage if arguments are wrong.
 *
 * @param argc  Number of input arguments
 * @param argv  Array of input arguments
 **/
static void processArgs(int argc, char *argv[])
{
  int result = register_vdo_status_codes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         uds_string_error(result, errorBuffer, ERRBUF_SIZE));
  }

  int c;
  while ((c = getopt_long(argc, argv, "hV", options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", helpString);
      exit(0);

    case 'o':
      result = parseSize(optarg, false, &offset);
      if (result != VDO_SUCCESS) {
        warnx("invalid offset: %s", optarg);
        usage(argv[0]);
      }

      if (((offset / VDO_BLOCK_SIZE) * VDO_BLOCK_SIZE) != offset) {
        errx(1, "offset must be a multiple of 4KB");
      }

      offset /= VDO_BLOCK_SIZE;
      break;

    case 'V':
      printf("%s version is: %s\n", argv[0], CURRENT_VERSION);
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

  fileName = argv[optind++];
}

/**
 * Stringify an error code.
 *
 * @param result  The error code
 *
 * @return The error message associated with the error code
 **/
static const char *resultString(int result) {
  return uds_string_error(result, errorBuffer, ERRBUF_SIZE);
}

/**
 * Generate a geometry based on index parameters.
 *
 * @param memory  The amount of index memory
 * @param sparse  Whether or not the index is sparse
 *
 * @return VDO_SUCCESS or an error
 **/
static int generateGeometry(const uds_memory_config_size_t memory, bool sparse)
{
  Candidate *candidate = &candidates[candidateCount];
  candidate->sparse    = sparse;
  if (memory == UDS_MEMORY_CONFIG_256MB) {
    sprintf(candidate->memoryString, "0.25");
  } else if (memory == UDS_MEMORY_CONFIG_512MB) {
    sprintf(candidate->memoryString, "0.5");
  } else if (memory == UDS_MEMORY_CONFIG_768MB) {
    sprintf(candidate->memoryString, "0.75");
  } else {
    sprintf(candidate->memoryString, "%d", memory);
  }

  UdsConfigStrings configStrings;
  memset(&configStrings, 0, sizeof(configStrings));
  configStrings.memorySize = candidate->memoryString;
  if (sparse) {
    configStrings.sparse = "1";
  }

  struct index_config indexConfig;
  int result = parseIndexConfig(&configStrings, &indexConfig);
  if (result != UDS_SUCCESS) {
    warnx("parseIndexConfig for memory %s%s failed: %s",
          candidate->memoryString, (sparse ? ", sparse" : ""),
          resultString(result));
    return result;
  }

  result = initialize_volume_geometry(current_time_us(), &uuid, &indexConfig,
                                      &candidate->geometry);
  if (result != VDO_SUCCESS) {
    warnx("failed to generate geometry for memory %s%s: %s",
          candidate->memoryString, (sparse ? ", sparse" : ""),
          resultString(result));
    return result;
  }

  return VDO_SUCCESS;
}

/**
 * Try to find a valid super block corresponding to a given index
 * configuration.
 *
 * @param memory  The memory size of the index
 * @param sparse  Whether or not the index is sparse
 *
 * @return <code>true</code> if a valid super block was found for this config
 **/
static bool tryUDSConfig(const uds_memory_config_size_t memory, bool sparse)
{
  Candidate *candidate = &candidates[candidateCount];
  if (generateGeometry(memory, sparse) != VDO_SUCCESS) {
    return false;
  }

  if ((offset != 0)
      && (get_data_region_offset(candidate->geometry) != offset)) {
    return false;
  }

  if (loadVDOWithGeometry(fileLayer, &candidate->geometry, false,
                          &candidate->vdo) != VDO_SUCCESS) {
    return false;
  }

  if (validate_vdo_config(&candidate->vdo->states.vdo.config, physicalSize,
                          true) != VDO_SUCCESS) {
    freeUserVDO(&candidate->vdo);
    return false;
  }

  struct block_map_state_2_0 map = candidate->vdo->states.block_map;
  for (block_count_t root = 0; root < map.root_count; root++) {
    int result = fileLayer->reader(fileLayer, map.root_origin + root, 1,
                                   blockBuffer);
    if (result != VDO_SUCCESS) {
      warnx("candidate block map root at %" PRIu64 " unreadable: %s",
            map.root_origin + root, resultString(result));
      return false;
    }

    enum block_map_page_validity validity
      = validate_block_map_page((struct block_map_page *) blockBuffer,
                                candidate->vdo->states.vdo.nonce,
                                map.root_origin + root);
    if (validity == BLOCK_MAP_PAGE_VALID) {
      printf("Found candidate super block at block %" PRIu64
             " (index memory %sGB%s)\n",
             get_data_region_offset(candidate->geometry),
             candidate->memoryString, (sparse ? ", sparse" : ""));
      return true;
    }
  }

  return false;
}

/**
 * Find all the super block candidates.
 **/
static void findSuperBlocks(void)
{
  const uds_memory_config_size_t smallSizes[] = {
    UDS_MEMORY_CONFIG_256MB,
    UDS_MEMORY_CONFIG_512MB,
    UDS_MEMORY_CONFIG_768MB,
  };

  bool trySparse = true;
  for (unsigned int i = 0; i < UDS_MEMORY_CONFIG_MAX; i++) {
    const uds_memory_config_size_t memory = ((i < 3) ? smallSizes[i] : i - 2);
    Candidate *candidate = &candidates[candidateCount];
    if (tryUDSConfig(memory, false)) {
      candidateCount++;
    } else if (get_data_region_offset(candidate->geometry) > physicalSize) {
      return;
    }

    if (trySparse && tryUDSConfig(memory, true)) {
      candidateCount++;
    } else if (get_data_region_offset(candidate->geometry) > physicalSize) {
      trySparse = false;
    }
  }
}

/**
 * Rewrite the geometry based on the one and only valid super block we found.
 *
 * @param candidate  The candidate geometry
 **/
static void rewriteGeometry(Candidate *candidate)
{
  candidate->geometry.nonce = candidate->vdo->states.vdo.nonce;
  freeUserVDO(&candidate->vdo);

  int result = write_volume_geometry(fileLayer, &candidate->geometry);
  if (result != VDO_SUCCESS) {
    errx(result, "Failed to write new geometry: %s", resultString(result));
  }
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  int result = register_vdo_status_codes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         uds_string_error(result, errorBuffer, ERRBUF_SIZE));
  }

  processArgs(argc, argv);

  result = makeFileLayer(fileName, 0, &fileLayer);
  if (result != VDO_SUCCESS) {
    errx(result, "Failed to open VDO backing store '%s' with %s",
         fileName, resultString(result));
  }

  result = fileLayer->allocateIOBuffer(fileLayer, VDO_BLOCK_SIZE,
                                       "block buffer", &blockBuffer);
  if (result != VDO_SUCCESS) {
    errx(result, "Failed to allocate block buffer: %s", resultString(result));
  }

  physicalSize = fileLayer->getBlockCount(fileLayer);

  if (offset > physicalSize) {
    errx(1, "Specified super block offset %" PRIu64
         " is beyond the end of the device", offset);
  }

  uuid_generate(uuid);

  findSuperBlocks();

  if (candidateCount == 1) {
    rewriteGeometry(&candidates[0]);
  } else if (candidateCount > 1) {
    printf("Found multiple candidate super blocks:\n");
    for (int i = 0; i < candidateCount; i++) {
      Candidate *candidate = &candidates[i];
      printf("offset: %" PRIu64 ", index memory %s%s\n",
             get_data_region_offset(candidate->geometry) * VDO_BLOCK_SIZE,
             candidate->memoryString, (candidate->sparse ?  ", sparse" : ""));
      freeUserVDO(&candidate->vdo);
    }

    printf("\n"
           "Rerun vdoRegenerateGeometry with the --offset parameter to select"
           "\na candidate\n");
  }

  FREE(blockBuffer);
  fileLayer->destroy(&fileLayer);

  if (candidateCount == 0) {
    errx(1, "No valid super block was found on %s", fileName);
  }

  exit((candidateCount == 1) ? 0 : 1);
}
