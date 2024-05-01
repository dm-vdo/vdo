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

#ifndef ASM_UNALIGNED_H
#define ASM_UNALIGNED_H

#include <asm/byteorder.h>
#include <linux/types.h>

/* Type safe comparison macros, similar to the ones in linux/kernel.h. */

/*
 * If pointers to types are comparable (without dereferencing them and
 * potentially causing side effects) then types are the same.
 */
#define TYPECHECK(x, y) (!!(sizeof((typeof(x) *) 1 == (typeof(y) *) 1)))
#define CONSTCHECK(x, y) (__builtin_constant_p(x) && __builtin_constant_p(y))

/* It takes two levels of macro expansion to compose the unique temp names. */
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define UNIQUE_ID(a) CONCAT(_UNIQUE_, CONCAT(a, __COUNTER__))

#define SAFE_COMPARE(x, y, unique_x, unique_y, op)          \
	__extension__({                                     \
		typeof(x) unique_x = (x);                   \
		typeof(y) unique_y = (y);                   \
		unique_x op unique_y ? unique_x : unique_y; \
	})

#define COMPARE(x, y, op)                              \
	__builtin_choose_expr(                         \
		(TYPECHECK(x, y) && CONSTCHECK(x, y)), \
		(((x) op(y)) ? (x) : (y)),             \
		SAFE_COMPARE(x, y, UNIQUE_ID(x_), UNIQUE_ID(y_), op))

#define min(x, y) COMPARE(x, y, <)
#define max(x, y) COMPARE(x, y, >)

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

#endif /* ASM_UNALIGNED_H */
