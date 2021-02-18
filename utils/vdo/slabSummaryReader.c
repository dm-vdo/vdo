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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/slabSummaryReader.c#8 $
 */

#include "slabSummaryReader.h"

#include <err.h>

#include "memoryAlloc.h"

#include "physicalLayer.h"
#include "slabDepotFormat.h"
#include "slabSummaryFormat.h"
#include "statusCodes.h"
#include "types.h"
#include "vdoComponentStates.h"

#include "userVDO.h"

/**********************************************************************/
int readSlabSummary(UserVDO *vdo, struct slab_summary_entry **entriesPtr)
{
  zone_count_t zones = vdo->states.slab_depot.zone_count;
  if (zones == 0) {
    return VDO_SUCCESS;
  }

  struct slab_summary_entry *entries;
  block_count_t summary_blocks = get_slab_summary_zone_size(VDO_BLOCK_SIZE);
  int result = vdo->layer->allocateIOBuffer(vdo->layer,
                                            summary_blocks * VDO_BLOCK_SIZE,
                                            "slab summary entries",
                                            (char **) &entries);
  if (result != VDO_SUCCESS) {
    warnx("Could not create in-memory slab summary");
    return result;
  }

  struct partition *slab_summary_partition;
  result = get_partition(vdo->states.layout, SLAB_SUMMARY_PARTITION,
                         &slab_summary_partition);
  if (result != VDO_SUCCESS) {
    warnx("Could not find slab summary partition");
    return result;
  }

  physical_block_number_t origin
    = get_fixed_layout_partition_offset(slab_summary_partition);
  result = vdo->layer->reader(vdo->layer, origin, summary_blocks,
                              (char *) entries);
  if (result != VDO_SUCCESS) {
    warnx("Could not read summary data");
    FREE(entries);
    return result;
  }

  // If there is more than one zone, read and combine the other zone's data
  // with the data already read from the first zone.
  if (zones > 1) {
    struct slab_summary_entry *buffer;
    result = vdo->layer->allocateIOBuffer(vdo->layer,
                                          summary_blocks * VDO_BLOCK_SIZE,
                                          "slab summary entries",
                                          (char **) &buffer);
    if (result != VDO_SUCCESS) {
      warnx("Could not create slab summary buffer");
      FREE(entries);
      return result;
    }

    for (zone_count_t zone = 1; zone < zones; zone++) {
      origin += summary_blocks;
      result = vdo->layer->reader(vdo->layer, origin, summary_blocks,
                                  (char *) buffer);
      if (result != VDO_SUCCESS) {
        warnx("Could not read summary data");
        FREE(buffer);
        FREE(entries);
        return result;
      }

      for (slab_count_t entry_number = zone; entry_number < MAX_SLABS;
           entry_number += zones) {
        memcpy(entries + entry_number, buffer + entry_number,
               sizeof(struct slab_summary_entry));
      }
    }

    FREE(buffer);
  }

  *entriesPtr = entries;
  return VDO_SUCCESS;
}
