/*
 * Copyright (c) 2017 Red Hat, Inc.
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
 * $Id: //eng/vdo-releases/magnesium/src/c++/vdo/user/parseUtils.h#1 $
 */

#ifndef PARSE_UTILS_H
#define PARSE_UTILS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Parse a string argument as an unsigned int.
 *
 * @param [in]  arg      The argument to parse
 * @param [in]  lowest   The lowest allowed value
 * @param [in]  highest  The highest allowed value
 * @param [out] sizePtr  A pointer to return the parsed integer.
 *
 * @return VDO_SUCCESS or VDO_OUT_OF_RANGE.
 **/
int parseUInt(const char   *arg,
              unsigned int  lowest,
              unsigned int  highest,
              unsigned int *numPtr);

/**
 * Parse a string argument as a uint64_t.
 *
 * @param [in]  arg      The argument to parse
 * @param [out] sizePtr  A pointer to return the parsed uint64_t
 *
 * @return VDO_SUCCESS or VDO_OUT_OF_RANGE.
 **/
int parseUInt64T(const char  *arg, uint64_t *numPtr);

/**
 * Parse a string argument as a size, optionally using LVM's concept
 * of size suffixes.
 *
 * @param [in]  arg      The argument to parse
 * @param [in]  lvmMode  Whether to parse suffixes as LVM or SI.
 * @param [out] sizePtr  A pointer to return the parsed size, in bytes
 *
 * @return VDO_SUCCESS or VDO_OUT_OF_RANGE.
 **/
int parseSize(const char *arg, bool lvmMode, uint64_t *sizePtr);

#endif // PARSE_UTILS_H
