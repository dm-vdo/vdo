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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoVolumeUtils.c#1 $
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
int loadSlabSummarySync(VDO *vdo, SlabSummary **summaryPtr)
{
  Partition *slabSummaryPartition = getVDOPartition(vdo->layout,
                                                    SLAB_SUMMARY_PARTITION);
  SlabDepot *depot = vdo->depot;
  ThreadConfig *threadConfig;
  int result = makeOneThreadConfig(&threadConfig);
  if (result != VDO_SUCCESS) {
    warnx("Could not create thread config");
    return result;
  }

  SlabSummary *summary = NULL;
  result = makeSlabSummary(vdo->layer, slabSummaryPartition, threadConfig,
                           depot->slabSizeShift, depot->slabConfig.dataBlocks,
                           NULL, &summary);
  if (result != VDO_SUCCESS) {
    warnx("Could not create in-memory slab summary");
  }
  freeThreadConfig(&threadConfig);
  if (result != VDO_SUCCESS) {
    return result;
  }

  PhysicalBlockNumber origin
    = getFixedLayoutPartitionOffset(slabSummaryPartition);
  result = vdo->layer->reader(vdo->layer, origin,
                              getSlabSummarySize(VDO_BLOCK_SIZE),
                              (char *) summary->entries, NULL);
  if (result != VDO_SUCCESS) {
    warnx("Could not read summary data");
    return result;
  }

  summary->zonesToCombine = depot->oldZoneCount;
  combineZones(summary);
  *summaryPtr = summary;
  return VDO_SUCCESS;
}
