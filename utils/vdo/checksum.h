/* SPDX-License-Identifier: GPL-2.0-only */
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

#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <zlib.h>

/**
 * A function to calculate a CRC-32 checksum.
 *
 * @param buffer  The data to  checksum
 * @param length  The length of the data
 *
 * @return The checksum
 **/
static inline uint32_t vdo_crc32(const byte *buffer, size_t length)
{
	/*
	 * Different from the kernelspace wrapper in vdo.h, because the kernel
	 * implementation doesn't precondition or postcondition the data; the
	 * userspace implementation does. So, despite the difference in these
	 * two implementations, they actually do the same checksum.
	 */
	return crc32(~0L, buffer, length);
}
#endif /* CHECKSUM_H */
