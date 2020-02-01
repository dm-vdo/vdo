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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/fileIORegion.h#1 $
 */

#ifndef FILE_IO_REGION_H
#define FILE_IO_REGION_H

#include "ioFactory.h"
#include "ioRegion.h"
#include "fileUtils.h"

/**
 * Make an IORegion using an open file descriptor.
 *
 * @param [in]  factory    The IOFactory holding the open file descriptor.
 * @param [in]  fd         The file descriptor.
 * @param [in]  access     The access kind for the file.
 * @param [in]  offset     The byte offset to the start of the region.
 * @param [in]  size       Size of the file region (in bytes).
 * @param [out] regionPtr  The new region.
 *
 * @return UDS_SUCCESS or an error code.
 **/
int makeFileRegion(IOFactory   *factory,
                   int          fd,
                   FileAccess   access,
                   off_t        offset,
                   size_t       size,
                   IORegion   **regionPtr)
  __attribute__((warn_unused_result));

#endif // FILE_IO_REGION_H
