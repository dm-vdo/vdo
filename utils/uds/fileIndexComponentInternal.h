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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/fileIndexComponentInternal.h#1 $
 */

#ifndef FILE_INDEX_COMPONENT_INTERNAL_H
#define FILE_INDEX_COMPONENT_INTERNAL_H

#include "fileIndexComponent.h"
#include "indexComponentInternal.h"

#include "compiler.h"
#include "permassert.h"
#include "util/pathBuffer.h"

/**
 * The structure representing a savable (and loadable) part of an index.
 **/
typedef struct fileIndexComponent {
  IndexComponent             common;        // IndexComponent part
  PathBuffer                 readPath;      // Path to read from
  PathBuffer                 writePath;     // Path to write to
} FileIndexComponent;

/*****************************************************************************/
static INLINE FileIndexComponent *asFileIndexComponent(IndexComponent *comp)
{
  return container_of(comp, FileIndexComponent, common);
}

#endif // FILE_INDEX_COMPONENT_INTERNAL_H
