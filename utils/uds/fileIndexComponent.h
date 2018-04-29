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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/fileIndexComponent.h#1 $
 */

#ifndef FILE_INDEX_COMPONENT_H
#define FILE_INDEX_COMPONENT_H

#include "indexComponent.h"

/**
 * Make a file index component.
 *
 * @param info          The component info
 * @param readDir       The location for reading
 * @param writeDir      The location for writing
 * @param zoneCount     The number of zones used in writing
 * @param data          The component structure
 * @param context       The load/save context of the component
 * @param componentPtr  A pointer to hold the component
 *
 * @return UDS_SUCCESS or an error code
 **/
int makeFileIndexComponent(const IndexComponentInfo  *info,
                           const char                *readDir,
                           const char                *writeDir,
                           unsigned int               zoneCount,
                           void                      *data,
                           void                      *context,
                           IndexComponent           **componentPtr)
  __attribute__((warn_unused_result));

/**
 * Renames the written file to the name expected for reading.
 * Used by writeSingleFileIndexComponent().
 *
 * @param component     The index component, which must be one created by
 *                        makeFileIndexComponent().
 *
 * @return UDS_SUCCESS or an error code.
 **/
int makeLastComponentSaveReadable(IndexComponent *component)
  __attribute__((warn_unused_result));

#endif // FILE_INDEX_COMPONENT_H
