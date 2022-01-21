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
 * $Id: //eng/vdo-releases/sulfur/src/c++/vdo/user/vdoVolumeUtils.h#2 $
 */

#ifndef VDO_VOLUME_UTILS_H
#define VDO_VOLUME_UTILS_H

#include "types.h"

#include "userVDO.h"

/**
 * Load a VDO from a file.
 *
 * @param [in]  filename  The file name
 * @param [in]  readOnly  Whether the layer should be read-only.
 * @param [out] vdoPtr    A pointer to hold the VDO
 *
 * @return VDO_SUCCESS or an error code
 **/
int __must_check
makeVDOFromFile(const char *filename, bool readOnly, UserVDO **vdoPtr);

/**
 * Load a VDO from a file without validating the config.
 *
 * @param [in]  filename  The file name
 * @param [out] vdoPtr    A pointer to hold the VDO
 *
 * @return VDO_SUCCESS or an error code
 **/
int __must_check
readVDOWithoutValidation(const char *filename, UserVDO **vdoPtr);

/**
 * Free the VDO made with makeVDOFromFile().
 *
 * @param vdoPtr  The pointer to the VDO to free
 **/
void freeVDOFromFile(UserVDO **vdoPtr);

#endif // VDO_VOLUME_UTILS_H
