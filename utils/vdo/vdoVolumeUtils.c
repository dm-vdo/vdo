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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoVolumeUtils.c#26 $
 */

#include "vdoVolumeUtils.h"

#include <err.h>

#include "permassert.h"

#include "fileLayer.h"
#include "userVDO.h"

static char errBuf[ERRBUF_SIZE];

/**
 * Load a VDO from a file.
 *
 * @param [in]  filename        The file name
 * @param [in]  readOnly        Whether the layer should be read-only.
 * @param [in]  validateConfig  Whether the VDO should validate its config
 * @param [out] vdoPtr          A pointer to hold the VDO
 *
 * @return VDO_SUCCESS or an error code
 **/
static int __must_check loadVDOFromFile(const char *filename,
					bool readOnly,
					bool validateConfig,
					UserVDO **vdoPtr)
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
    warnx("Failed to make FileLayer from '%s' with %s",
          filename, uds_string_error(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  // Create the VDO.
  UserVDO *vdo;
  result = loadVDO(layer, validateConfig, &vdo);
  if (result != VDO_SUCCESS) {
    layer->destroy(&layer);
    warnx("loading VDO failed with: %s",
          uds_string_error(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  *vdoPtr = vdo;
  return VDO_SUCCESS;
}

/**********************************************************************/
int makeVDOFromFile(const char *filename, bool readOnly, UserVDO **vdoPtr)
{
  return loadVDOFromFile(filename, readOnly, true, vdoPtr);
}

/**********************************************************************/
int readVDOWithoutValidation(const char *filename, UserVDO **vdoPtr)
{
  return loadVDOFromFile(filename, true, false, vdoPtr);
}

/**********************************************************************/
void freeVDOFromFile(UserVDO **vdoPtr)
{
  UserVDO *vdo = *vdoPtr;
  if (vdo == NULL) {
    return;
  }

  PhysicalLayer *layer = vdo->layer;
  freeUserVDO(&vdo);
  layer->destroy(&layer);
  *vdoPtr = NULL;
}
