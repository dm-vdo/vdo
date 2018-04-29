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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/directoryReader.h#1 $
 */

#ifndef DIRECTORY_READER_H
#define DIRECTORY_READER_H

#include <dirent.h>

#include "common.h"
#include "typeDefs.h"

/*
 * A function which processes directory entries. It's arguments are a
 * directory entry, the name of the directory being read, a context,
 * and a pointer to hold an error code. The function returns true if
 * the reader should stop reading the directory.
 */
typedef bool DirectoryEntryProcessor(struct dirent *entry,
                                     const char    *directory,
                                     void          *context,
                                     int           *result);

/**
 * Read a directory, passing each entry to a supplied reader function.
 *
 * @param path             The path of the directory to read
 * @param directoryType    The type of directory (for error reporting)
 * @param entryProcessor   The function to call for each entry in the directory
 * @param context          The context to pass to the entry processor
 *
 * @return UDS_SUCCESS or an error code
 **/
int readDirectory(const char              *path,
                  const char              *directoryType,
                  DirectoryEntryProcessor *entryProcessor,
                  void                    *context)
  __attribute__((warn_unused_result));

#endif /* DIRECTORY_READER_H */
