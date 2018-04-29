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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/indexLayoutLinuxUser.c#1 $
 */

#include "errors.h"
#include "fileIORegion.h"
#include "indexLayout.h"
#include "indexLayoutParser.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "multiFileLayout.h"
#include "singleFileLayout.h"
#include "uds.h"

/*****************************************************************************/
int makeIndexLayout(const char              *name,
                    bool                     newLayout,
                    const UdsConfiguration   config,
                    IndexLayout            **layoutPtr)
{
  char     *file   = NULL;
  char     *dir    = NULL;
  uint64_t  offset = 0;
  uint64_t  size   = 0;

  LayoutParameter parameterTable[] = {
    { "directory", LP_STRING | LP_DEFAULT, { .str = &dir    } },
    { "file",      LP_STRING,              { .str = &file   } },
    { "size",      LP_UINT64,              { .num = &size   } },
    { "offset",    LP_UINT64,              { .num = &offset } },
  };

  unsigned int numParameters
    = sizeof(parameterTable) / sizeof(*parameterTable);

  char *params = NULL;
  int result = duplicateString(name, "makeIndexLayout parameters", &params);
  if (result != UDS_SUCCESS) {
    return result;
  }

  // note dir and file will be set to memory owned by params
  //
  result = parseLayoutString(params, parameterTable, numParameters);
  if (result != UDS_SUCCESS) {
    FREE(params);
    return result;
  }

  if (dir && file) {
    FREE(params);
    return logErrorWithStringError(UDS_INDEX_NAME_REQUIRED,
                                   "ambiguous index parameters, "
                                   "both file and directory supplied");
  }

  if (dir) {
    result = makeMultiFileLayout(dir, layoutPtr);
    FREE(params);
    return result;
  }

  if (!file) {
    FREE(params);
    return logErrorWithStringError(UDS_INDEX_NAME_REQUIRED,
                                   "no index specified");
  }
  if (newLayout && size == 0) {
    result = udsComputeIndexSize(config, 0, &size);
    if (result != UDS_SUCCESS) {
      FREE(params);
      return result;
    }
  }

  IORegion *region = NULL;
  if (newLayout) {
    result = openFileRegion(file, FU_CREATE_READ_WRITE, &region);
    if (result == UDS_SUCCESS) {
      result = setFileRegionLimit(region, offset + size);
    }
  } else {
    result = openFileRegion(file, FU_READ_WRITE, &region);
  }

  FREE(params);
  if (result != UDS_SUCCESS) {
    closeIORegion(&region);
    return result;
  }


  if (newLayout) {
    result = createSingleFileLayout(region, offset, size, config, layoutPtr);
  } else {
    result = loadSingleFileLayout(region, offset, layoutPtr);
  }

  if (result != UDS_SUCCESS) {
    closeIORegion(&region);
  }
  return result;
}
