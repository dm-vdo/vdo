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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoVolumeUtils.c#7 $
 */

#include "vdoVolumeUtils.h"

#include <err.h>

#include "constants.h"
#include "fixedLayout.h"
#include "slab.h"
#include "slabDepotInternals.h"
#include "slabSummaryInternals.h"
#include "types.h"
#include "vdoInternal.h"
#include "vdoLayout.h"
#include "vdoLoad.h"

#include "fileLayer.h"

/**
 * Load a VDO from a file.
 *
 * @param [in]  filename        The file name
 * @param [in]  readOnly        Whether the layer should be read-only.
 * @param [in]  validateConfig  Whether the VDO should validate its config
 * @param [in]  decoder         The VDO decoder to use, if NULL, the default
 *                              decoder will be used
 * @param [out] vdoPtr          A pointer to hold the VDO
 *
 * @return VDO_SUCCESS or an error code
 **/
__attribute__((warn_unused_result))
static int loadVDOFromFile(const char  *filename,
                           bool         readOnly,
                           bool         validateConfig,
                           VDODecoder  *decoder,
                           VDO        **vdoPtr)
{
  int result = ASSERT(validateConfig || readOnly,
                      "Cannot make a writable VDO"
                      " without validating its config");
  if (result != UDS_SUCCESS) {
    return result;
  }

  PhysicalLayer *layer;
  if (readOnly) {
    result = makeReadOnlyFileLayer(filename, &layer);
  } else {
    result = makeFileLayer(filename, 0, &layer);
  }

  if (result != VDO_SUCCESS) {
    char errBuf[ERRBUF_SIZE];
    warnx("Failed to make FileLayer from '%s' with %s",
          filename, stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  // Create the VDO.
  VDO *vdo;
  result = loadVDO(layer, validateConfig, decoder, &vdo);
  if (result != VDO_SUCCESS) {
    layer->destroy(&layer);
    char errBuf[ERRBUF_SIZE];
    warnx("allocateVDO failed for '%s' with %s",
          filename, stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  *vdoPtr = vdo;
  return VDO_SUCCESS;
}

/**********************************************************************/
int makeVDOFromFile(const char *filename, bool readOnly, VDO **vdoPtr)
{
  return loadVDOFromFile(filename, readOnly, true, NULL, vdoPtr);
}

/**********************************************************************/
int readVDOWithoutValidation(const char *filename, VDO **vdoPtr)
{
  return loadVDOFromFile(filename, true, false, NULL, vdoPtr);
}

/**********************************************************************/
void freeVDOFromFile(VDO **vdoPtr)
{
  if (*vdoPtr == NULL) {
    return;
  }

  PhysicalLayer *layer = (*vdoPtr)->layer;
  freeVDO(vdoPtr);
  layer->destroy(&layer);
}

/**********************************************************************/
int loadSlabSummarySync(VDO *vdo, struct slab_summary **summaryPtr)
{
  struct partition *slabSummaryPartition
    = getVDOPartition(vdo->layout, SLAB_SUMMARY_PARTITION);
  struct slab_depot *depot = vdo->depot;
  ThreadConfig *threadConfig;
  int result = makeOneThreadConfig(&threadConfig);
  if (result != VDO_SUCCESS) {
    warnx("Could not create thread config");
    return result;
  }

  struct slab_summary *summary = NULL;
  result = make_slab_summary(vdo->layer, slabSummaryPartition, threadConfig,
                             depot->slab_size_shift,
                             depot->slab_config.dataBlocks,
                             NULL, &summary);
  if (result != VDO_SUCCESS) {
    warnx("Could not create in-memory slab summary");
  }
  freeThreadConfig(&threadConfig);
  if (result != VDO_SUCCESS) {
    return result;
  }

  PhysicalBlockNumber origin
    = get_fixed_layout_partition_offset(slabSummaryPartition);
  result = vdo->layer->reader(vdo->layer, origin,
                              get_slab_summary_size(VDO_BLOCK_SIZE),
                              (char *) summary->entries, NULL);
  if (result != VDO_SUCCESS) {
    warnx("Could not read summary data");
    return result;
  }

  summary->zones_to_combine = depot->old_zone_count;
  combine_zones(summary);
  *summaryPtr = summary;
  return VDO_SUCCESS;
}
