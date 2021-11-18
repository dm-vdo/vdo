/*
 * Copyright Red Hat
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
 */

#ifndef FILE_IO_REGION_H
#define FILE_IO_REGION_H

#include "io-factory.h"
#include "ioRegion.h"
#include "fileUtils.h"

/**
 * Make an IO region using an open file descriptor.
 *
 * @param [in]  factory    The IO factory holding the open file descriptor.
 * @param [in]  fd         The file descriptor.
 * @param [in]  access     The access kind for the file.
 * @param [in]  offset     The byte offset to the start of the region.
 * @param [in]  size       Size of the file region (in bytes).
 * @param [out] region_ptr The new region.
 *
 * @return UDS_SUCCESS or an error code.
 **/
int __must_check make_file_region(struct io_factory *factory,
				  int fd,
				  enum file_access access,
				  off_t offset,
				  size_t size,
				  struct io_region **region_ptr);

#endif // FILE_IO_REGION_H
