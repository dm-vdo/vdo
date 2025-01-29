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

#ifndef LINUX_UNALIGNED_H
#define LINUX_UNALIGNED_H

#include <asm/byteorder.h>
#include <linux/types.h>

/* Type safe comparison macros, similar to the ones in linux/minmax.h. */

/*
 * If pointers to types are comparable (without dereferencing them and
 * potentially causing side effects) then types are the same.
 */
 #define __typecheck(x, y) \
	(!!(sizeof((typeof(x) *)1 == (typeof(y) *)1)))

/* 
 * Hack for VDO to replace use of the kernel's __is_constexpr() in __cmp_ macros.
 * VDO cannot use __is_constexpr() due to it relying on a GCC extension to allow sizeof(void).
 */
#define __constcheck(x, y) \
	(__builtin_constant_p(x) && __builtin_constant_p(y))

/* It takes two levels of macro expansion to compose the unique temp names. */
#define ___PASTE(a,b) a##b
#define __PASTE(a,b) ___PASTE(a,b)
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

/* Defined in linux/minmax.h */
#define __cmp_op_min <
#define __cmp_op_max >

#define __cmp(op, x, y)	((x) __cmp_op_##op (y) ? (x) : (y))

#define __cmp_once(op, x, y, unique_x, unique_y) \
	__extension__({                          \
		typeof(x) unique_x = (x);        \
		typeof(y) unique_y = (y);        \
		__cmp(op, unique_x, unique_y);   \
	})

#define __careful_cmp(op, x, y)                            \
	__builtin_choose_expr(                             \
		(__typecheck(x, y) && __constcheck(x, y)), \
		__cmp(op, x, y),                           \
		__cmp_once(op, x, y, __UNIQUE_ID(x_), __UNIQUE_ID(y_)))

#define min(x, y) __careful_cmp(min, x, y)
#define max(x, y) __careful_cmp(max, x, y)

/* Defined in linux/minmax.h */
#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

/* Defined in linux/math.h */
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

/* Defined in asm/unaligned.h */
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

#endif /* LINUX_UNALIGNED_H */
