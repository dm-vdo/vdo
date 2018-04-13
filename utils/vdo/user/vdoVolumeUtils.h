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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoVolumeUtils.h#1 $
 */

#ifndef VDO_VOLUME_UTILS_H
#define VDO_VOLUME_UTILS_H

#include "fixedLayout.h"
#include "types.h"
#include "vdoLoad.h"

/**
 * Load a VDO from a file.
 *
 * @param [in]  filename  The file name
 * @param [in]  readOnly  Whether the layer should be read-only.
 * @param [out] vdoPtr    A pointer to hold the VDO
 *
 * @return VDO_SUCCESS or an error code
 **/
int makeVDOFromFile(const char *filename, bool readOnly, VDO **vdoPtr)
  __attribute__((warn_unused_result));

/**
 * Load a VDO from a file without validating the config.
 *
 * @param [in]  filename  The file name
 * @param [out] vdoPtr    A pointer to hold the VDO
 *
 * @return VDO_SUCCESS or an error code
 **/
int readVDOWithoutValidation(const char *filename, VDO **vdoPtr)
  __attribute__((warn_unused_result));

/**
 * Free the VDO made with makeVDOFromFile().
 *
 * @param vdoPtr  The pointer to the VDO to free
 **/
void freeVDOFromFile(VDO **vdoPtr);

/**
 * Set up a slab summary and read the on-disk slab summary to populate it.
 *
 * @param [in]  vdo         The VDO, loaded synchronously
 * @param [out] summaryPtr  A pointer to return the loaded slab summary
 *
 * @return VDO_SUCCESS or an error code
 **/
int loadSlabSummarySync(VDO *vdo, SlabSummary **summaryPtr)
  __attribute__((warn_unused_result));

#endif // VDO_VOLUME_UTILS_H
