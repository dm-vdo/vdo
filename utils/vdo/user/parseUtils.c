/*
 * Copyright (c) 2017 Red Hat, Inc.
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
 * $Id: //eng/vdo-releases/magnesium/src/c++/vdo/user/parseUtils.c#1 $
 */

#include "parseUtils.h"

#include <errno.h>
#include <stdlib.h>

#include "statusCodes.h"

/**********************************************************************/
int parseUInt(const char   *arg,
              unsigned int  lowest,
              unsigned int  highest,
              unsigned int *numPtr)
{
  char *endPtr;
  errno = 0;
  unsigned long n = strtoul(arg, &endPtr, 0);
  if ((errno == ERANGE) || (errno == EINVAL) || (endPtr == arg) ||
      (*endPtr != '\0') || (n < lowest) || (n > highest)) {
    return VDO_OUT_OF_RANGE;
  }
  if (numPtr != NULL) {
    *numPtr = n;
  }
  return VDO_SUCCESS;
}

/**********************************************************************/
int parseUInt64T(const char *arg, uint64_t *numPtr)
{
  char *endPtr;
  errno = 0;
  unsigned long long n = strtoull(arg, &endPtr, 0);
  if ((errno == ERANGE) || (errno == EINVAL) || (endPtr == arg) ||
      (*endPtr != '\0')) {
    return VDO_OUT_OF_RANGE;
  }

  if (numPtr != NULL) {
    *numPtr = n;
  }

  return VDO_SUCCESS;
}

/**********************************************************************/
int parseSize(const char *arg, bool lvmMode, uint64_t *sizePtr)
{
  char *endPtr;
  errno = 0;
  unsigned long size = strtoul(arg, &endPtr, 0);
  if ((errno == ERANGE) || (errno == EINVAL) || (endPtr == arg)) {
    return VDO_OUT_OF_RANGE;
  }
  switch (*endPtr++) {
  case 'p':
  case 'P':
    if ((size & (-1L << 54)) != 0) {
      return VDO_OUT_OF_RANGE;
    }
    size *= 1024;
  case 't':
  case 'T':
    if ((size & (-1L << 54)) != 0) {
      return VDO_OUT_OF_RANGE;
    }
    size *= 1024;
  case 'g':
  case 'G':
    if ((size & (-1L << 54)) != 0) {
      return VDO_OUT_OF_RANGE;
    }
    size *= 1024;
  case 'm':
  case 'M':
    if ((size & (-1L << 54)) != 0) {
      return VDO_OUT_OF_RANGE;
    }
    size *= 1024;
  case 'k':
  case 'K':
    if ((size & (-1L << 54)) != 0) {
      return VDO_OUT_OF_RANGE;
    }
    size *= 1024;
  case 'b':
  case 'B':
    if (*endPtr != '\000') {
      return VDO_OUT_OF_RANGE;
    }
    break;
  case '\000':
    if (lvmMode) {
      if ((size & (-1L << 44)) != 0) {
        return VDO_OUT_OF_RANGE;
      }
      size *= 1048576;
    }
    break;
  default :
    return VDO_OUT_OF_RANGE;
    break;
  }
  *sizePtr = size;
  return VDO_SUCCESS;
}
