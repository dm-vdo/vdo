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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/parseUtils.c#2 $
 */

#include "parseUtils.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>

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
  if ((errno == ERANGE) || (errno == EINVAL)
      || (endPtr == arg) || (*endPtr != '\0')
      || (n < lowest) || (n > highest)) {
    return VDO_OUT_OF_RANGE;
  }
  if (numPtr != NULL) {
    *numPtr = n;
  }
  return VDO_SUCCESS;
}

/**
 * Return the binary exponent corresponding to a unit code.
 *
 * @param unitCode  The code, which is 'b' or 'B' for bytes, 'm' or 'M'
 *                  for megabytes, etc.
 *
 * @return The binary exponent corresponding to the code,
 *         or -1 if the code is not valid
 **/
static int getBinaryExponent(char unitCode)
{
  const char *UNIT_CODES = "BKMGTP";
  const char *where = index(UNIT_CODES, toupper(unitCode));
  if (where == NULL) {
    return -1;
  }
  // Each successive code is another factor of 2^10 bytes.
  return (10 * (where - UNIT_CODES));
}

/**********************************************************************/
int parseSize(const char *arg, bool lvmMode, uint64_t *sizePtr)
{
  char *endPtr;
  errno = 0;
  unsigned long long size = strtoull(arg, &endPtr, 0);
  if ((errno == ERANGE) || (errno == EINVAL) || (endPtr == arg)) {
    return VDO_OUT_OF_RANGE;
  }

  int exponent;
  if (*endPtr == '\0') {
    // No units specified; SI mode defaults to bytes, LVM mode to megabytes.
    exponent = lvmMode ? 20 : 0;
  } else {
    // Parse the unit code.
    exponent = getBinaryExponent(*endPtr++);
    if (exponent < 0) {
      return VDO_OUT_OF_RANGE;
    }
    if (*endPtr != '\0') {
      return VDO_OUT_OF_RANGE;
    }
  }

  // Scale the size by the specified units, checking for overflow.
  uint64_t actualSize = size << exponent;
  if (size != (actualSize >> exponent)) {
    return VDO_OUT_OF_RANGE;
  }

  *sizePtr = actualSize;
  return VDO_SUCCESS;
}

static int parseMem(char *string, uint32_t *sizePtr)
{
  UdsMemoryConfigSize mem;
  if (strcmp(string, "0.25") == 0) {
    mem = UDS_MEMORY_CONFIG_256MB;
  } else if (strcmp(string, "0.5") == 0) {
    mem = UDS_MEMORY_CONFIG_512MB;
  } else if (strcmp(string, "0.75") == 0) {
    mem = UDS_MEMORY_CONFIG_768MB;
  } else {
    unsigned long number;
    if (stringToUnsignedLong(string, &number) != UDS_SUCCESS) {
      return -EINVAL;
    }
    mem = number;
    if (mem != number) {
      return -EINVAL;
    }
  }
  *sizePtr = mem;
  return UDS_SUCCESS;
}

/**********************************************************************/
int parseIndexConfig(UdsConfigStrings *configStrings,
                     IndexConfig      *configPtr)
{
  IndexConfig config;
  memset(&config, 0, sizeof(config));

  config.mem = UDS_MEMORY_CONFIG_256MB;
  if (configStrings->memorySize != NULL) {
    int result = parseMem(configStrings->memorySize, &config.mem);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  if (configStrings->checkpointFrequency != NULL) {
    unsigned long number;
    int result = stringToUnsignedLong(configStrings->checkpointFrequency,
                                      &number);
    if (result != UDS_SUCCESS) {
      return result;
    }
    if (number != (unsigned int) number) {
      return UDS_OUT_OF_RANGE;
    }
    config.checkpointFrequency = number;
  }

  if (configStrings->sparse != NULL) {
    config.sparse = (strcmp(configStrings->sparse, "0") != 0);
  }

  *configPtr = config;
  return VDO_SUCCESS;
}
