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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/numericDefs.h#1 $
 */

#ifndef LINUX_USER_NUMERIC_DEFS_H
#define LINUX_USER_NUMERIC_DEFS_H 1

#include <asm/byteorder.h>
#include <stdint.h>

static inline uint16_t get_unaligned_le16(const void *p)
{
	return __le16_to_cpup((const __le16 *)p);
}

static inline uint32_t get_unaligned_le32(const void *p)
{
	return __le32_to_cpup((const __le32 *)p);
}

static inline uint64_t get_unaligned_le64(const void *p)
{
	return __le64_to_cpup((const __le64 *)p);
}

static inline uint16_t get_unaligned_be16(const void *p)
{
	return __be16_to_cpup((const __be16 *)p);
}

static inline uint32_t get_unaligned_be32(const void *p)
{
	return __be32_to_cpup((const __be32 *)p);
}

static inline uint64_t get_unaligned_be64(const void *p)
{
	return __be64_to_cpup((const __be64 *)p);
}

static inline void put_unaligned_le16(uint16_t val, void *p)
{
	*((__le16 *)p) = __cpu_to_le16(val);
}

static inline void put_unaligned_le32(uint32_t val, void *p)
{
	*((__le32 *)p) = __cpu_to_le32(val);
}

static inline void put_unaligned_le64(uint64_t val, void *p)
{
	*((__le64 *)p) = __cpu_to_le64(val);
}

static inline void put_unaligned_be16(uint16_t val, void *p)
{
	*((__be16 *)p) = __cpu_to_be16(val);
}

static inline void put_unaligned_be32(uint32_t val, void *p)
{
	*((__be32 *)p) = __cpu_to_be32(val);
}

static inline void put_unaligned_be64(uint64_t val, void *p)
{
	*((__be64 *)p) = __cpu_to_be64(val);
}

#endif /* LINUX_USER_NUMERIC_DEFS_H */
