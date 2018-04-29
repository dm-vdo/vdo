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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/directoryReader.c#1 $
 */

#include "directoryReader.h"

#include <unistd.h>

#include "errors.h"
#include "fileUtils.h"
#include "logger.h"
#include "memoryAlloc.h"

/**********************************************************************/
static int allocateDirentBuffer(const char *path, struct dirent **direntPtr)
{
  long maxFileNameLength;
  int result = loggingPathconf(path, _PC_NAME_MAX, __func__,
                               &maxFileNameLength);
  if (result != UDS_SUCCESS) {
    return result;
  }

  // If no limit is configured, use NAME_MAX
  size_t nameLength = ((maxFileNameLength == -1)
                       ? NAME_MAX
                       : maxFileNameLength);
  struct dirent *entry;
  result = ALLOCATE_EXTENDED(struct dirent, nameLength + 1, char,
                             "directory entry", &entry);
  if (result == UDS_SUCCESS) {
    *direntPtr = entry;
  }
  return result;
}

/**********************************************************************/
int readDirectory(const char              *path,
                  const char              *directoryType,
                  DirectoryEntryProcessor  entryProcessor,
                  void                    *context)
{
  DIR *directory;
  int  result    = openDirectory(path, directoryType, __func__, &directory);
  if (result != UDS_SUCCESS) {
    return result;
  }

  struct dirent *direntBuffer;
  result = allocateDirentBuffer(path, &direntBuffer);
  if (result != UDS_SUCCESS) {
    closeDirectory(directory, __func__);
    return result;
  }

  for (;;) {
    struct dirent *entry;
    result = readdir_r(directory, direntBuffer, &entry);
    if ((result != 0) || (entry == NULL)) {
      break;
    }
    if ((strcmp(entry->d_name, ".") == 0)
        || (strcmp(entry->d_name, "..") == 0)) {
      continue;
    }
    if (((*entryProcessor)(entry, path, context, &result))
        || (result != UDS_SUCCESS)) {
      break;
    }
  }

  closeDirectory(directory, __func__);
  FREE(direntBuffer);
  return result;
}
