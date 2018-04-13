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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/stringLinuxUser.c#1 $
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"
#include "logger.h"
#include "stringUtils.h"
#include "uds.h"

/**********************************************************************/
int stringToSignedLong(const char *nptr, long *num)
{
  if (nptr == NULL || *nptr == '\0') {
    return UDS_INVALID_ARGUMENT;
  }
  errno = 0;
  char *endptr;
  *num = strtol(nptr, &endptr, 10);
  if (*endptr != '\0') {
    return UDS_INVALID_ARGUMENT;
  }
  return errno;
}

/**********************************************************************/
int stringToUnsignedLong(const char *nptr, unsigned long *num)
{
  if (nptr == NULL || *nptr == '\0') {
    return UDS_INVALID_ARGUMENT;
  }
  errno = 0;
  char *endptr;
  *num = strtoul(nptr, &endptr, 10);
  if (*endptr != '\0') {
    return UDS_INVALID_ARGUMENT;
  }
  return errno;
}

/**********************************************************************/
int scanString(const char *what, int numItems, const char *str,
               const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  int result = vsscanf(str, fmt, args);
  va_end(args);
  if (result != numItems) {
    return logErrorWithStringError(UDS_INVALID_ARGUMENT,
                                   "%s could not convert %d items from "
                                   "string: %s%s%s", __func__, numItems,
                                   str,
                                   ((what != NULL) && (*what != '\0'))
                                   ? ": " : "",
                                   (what != NULL) ? what : "");
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
char *nextToken(char *str, const char *delims, char **state)
{
  return strtok_r(str, delims, state);
}

/*****************************************************************************/
int parseUint64(const char *str, uint64_t *num)
{
  char *end;
  errno = 0;
  unsigned long long temp = strtoull(str, &end, 10);
  // strtoull will always set end. On error, it could set errno to ERANGE or
  // EINVAL.  (It also returns ULLONG_MAX when setting errno to ERANGE.)
  if ((errno == ERANGE) || (errno == EINVAL) || (*end != '\0')) {
    return UDS_INVALID_ARGUMENT;
  }
  uint64_t n = temp;
  if (temp != (unsigned long long) n) {
    return UDS_INVALID_ARGUMENT;
  }

  *num = n;
  return UDS_SUCCESS;
}
