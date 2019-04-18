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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/fileLayer.h#1 $
 */

#ifndef FILE_LAYER_H
#define FILE_LAYER_H

#include "physicalLayer.h"

/**
 * Make a file layer implementation of a physical layer.
 *
 * @param [in]  name        the name of the underlying file
 * @param [in]  blockCount  the span of the file, in blocks
 * @param [out] layerPtr    the pointer to hold the result
 *
 * @return a success or error code
 **/
int makeFileLayer(const char     *name,
                  BlockCount      blockCount,
                  PhysicalLayer **layerPtr)
  __attribute__((warn_unused_result));

/**
 * Make a read only file layer implementation of a physical layer.
 *
 * @param [in]  name        the name of the underlying file
 * @param [out] layerPtr    the pointer to hold the result
 *
 * @return a success or error code
 **/
int makeReadOnlyFileLayer(const char *name, PhysicalLayer **layerPtr)
  __attribute__((warn_unused_result));

#endif // FILE_LAYER_H
