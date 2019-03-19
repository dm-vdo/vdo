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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/fileBufferedReader.h#1 $
 */

#ifndef FILE_BUFFERED_READER_H
#define FILE_BUFFERED_READER_H

#include "bufferedReaderInternals.h"

/**
 * Make a new file buffered reader.
 *
 * @param [in]  fd          The file descriptor for this buffer.
 * @param [out] readerPtr   The pointer to hold the newly allocated buffer.
 *
 * @return                  UDS_SUCCESS or error code.
 **/
int makeFileBufferedReader(int fd, BufferedReader **readerPtr)
  __attribute__((warn_unused_result));

/**
 * Open a file and make a new file buffer reader for it.
 *
 * @param [in]  path        The path to the file.
 * @param [out] readerPtr   The pointer to hold the newly allocated buffer.
 *
 * @return                  UDS_SUCCESS or error code.
 **/
int openFileBufferedReader(const char *path, BufferedReader **readerPtr)
  __attribute__((warn_unused_result));

#endif // FILE_BUFFERED_WRITER_H
