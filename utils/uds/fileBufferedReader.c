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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/fileBufferedReader.c#1 $
 */

#include "fileBufferedReader.h"

#include "bufferedReaderInternals.h"
#include "fileIORegion.h"
#include "memoryAlloc.h"

/*****************************************************************************/
int makeFileBufferedReader(int fd, BufferedReader **readerPtr)
{
  IORegion *region = NULL;
  int result = makeFileRegion(fd, FU_READ_ONLY, &region);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = makeBufferedReader(region, readerPtr);
  if (result != UDS_SUCCESS) {
    closeIORegion(&region);
    return result;
  }

  (*readerPtr)->close = true;
  return UDS_SUCCESS;
}

/*****************************************************************************/
int openFileBufferedReader(const char *path, BufferedReader **readerPtr)
{
  IORegion *region = NULL;
  int result = openFileRegion(path, FU_READ_ONLY, &region);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = makeBufferedReader(region, readerPtr);
  if (result != UDS_SUCCESS) {
    closeIORegion(&region);
    return result;
  }

  (*readerPtr)->close = true;
  return UDS_SUCCESS;
}
