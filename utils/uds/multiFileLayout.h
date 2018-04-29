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
 * $Id: //eng/uds-releases/flanders-rhel7.5/src/uds/multiFileLayout.h#1 $
 */

#ifndef MULTI_FILE_LAYOUT_H
#define MULTI_FILE_LAYOUT_H

#include "indexLayout.h"

/**
 * Construct a multi-file index layout as used in user-space Albireo.
 *
 * @param path          path to index directory
 * @param layoutPtr     where to store the new index layout
 *
 * @return UDS_SUCCESS or an error code.
 **/
int makeMultiFileLayout(const char *path, IndexLayout **layoutPtr);

#endif // MULTI_FILE_LAYOUT_H
