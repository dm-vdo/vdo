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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/fileIORegion.h#2 $
 */

#ifndef FILE_IO_REGION_H
#define FILE_IO_REGION_H

#include "ioRegion.h"
#include "fileUtils.h"

/**
 * Open a file as an IORegion.
 *
 * @param [in]  path            The pathname for the file.
 * @param [in]  access          The requested access kind.
 * @param [out] regionPtr       The new region.
 *
 * @return UDS_SUCCESS or an error code.
 **/
int openFileRegion(const char *path, FileAccess access, IORegion **regionPtr)
  __attribute__((warn_unused_result));

/**
 * Make an IORegion using an open file descriptor.
 *
 * @param [in]  fd              The file descriptor.
 * @param [in]  access          The access kind for the file.
 * @param [out] regionPtr       The new region.
 *
 * @return UDS_SUCCESS or an error code.
 **/
int makeFileRegion(int fd, FileAccess access, IORegion **regionPtr)
  __attribute__((warn_unused_result));

/**
 * Expand the underlying file to the size specified by limit if it is not
 * already big enough.
 *
 * @param region                A region created by makeFileRegion() or
 *                                openFileRegion().
 * @param limit                 The required minimum size of the file.
 *
 * @return UDS_SUCCESS or an error code.
 **/
int setFileRegionLimit(IORegion *region, off_t limit)
  __attribute__((warn_unused_result));

/**
 * Set the close behavior of a file IO region.
 *
 * @param region                An IORegion created by makeFileRegion() or
 *                                openFileRegion().
 * @param closeFile             If true, close the underlying file descriptor.
 **/
void setFileRegionCloseBehavior(IORegion *region, bool closeFile);

#endif // FILE_IO_REGION_H
