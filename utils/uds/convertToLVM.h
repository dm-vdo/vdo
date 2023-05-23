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
 *
 * $Id: //eng/uds-releases/jasper/src/uds/convertToLVM.h#5 $
 */

#include "uds.h"

/**
 * Shrinks a UDS index to give VDO space to allow for LVM metadata to
 * be prefixed while retaining as much deduplication as possible. This
 * is done by reducing the chapter count by one and moving the super
 * block and the configuration block to the end of the vacated space,
 * thereby freeing space equal to the size of a chapter at the
 * beginning of the index.
 * 
 * If the operation is successful, the UdsConfiguration
 * pointed to by the config argument will have been modified to
 * represent the new chapter configuration and the number of bytes in
 * a chapter will be returned in the location pointed to by the
 * chapterSize argument.
 *
 * @param name         The path to the device or file containing the
 *                     index with optional offset=# where # represents
 *                     an offset from the beginning of the device or
 *                     file to the start of the index.
 * @param freedSpace   The minimun amount of space to free at the start
 *                     of the device, in bytes. Must be a multiple of 4K.      
 * @param config       The UDS configuration of the index
 * @param chapterSize  A place to return the size in bytes of the
 *                     chapter that was eliminated
 * @return  UDS_SUCCESS or an error code
 */
int udsConvertToLVM(const char       *name,
                    size_t            freedSpace,
                    UdsConfiguration  config,
                    off_t            *chapterSize)
  __attribute__((warn_unused_result));

/**
 * Update the lvm offset that is stored in the layout when we repair
 * a broken conversion.
 * 
 * @param path            The path to the device or file containing the index
 * @param indexOffset     The offset in the device to load/save the index at
 * @param newStartOffset  The new value for the index superblock's
 *                        startOffset field
 *
 * @return  UDS_SUCCESS or an error code
 */
int udsRepairConvertToLVM(const char *path,
                          size_t      indexOffset,
                          size_t      newStartOffset)
  __attribute__((warn_unused_result));
