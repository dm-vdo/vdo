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
 * $Id: //eng/uds-releases/lisa/src/uds/compiler.h#2 $
 */

#ifndef COMMON_COMPILER_H
#define COMMON_COMPILER_H


// Count the elements in a static array while attempting to catch some type
// errors. (See http://stackoverflow.com/a/1598827 for an explanation.)
#define ARRAY_SIZE(x)					\
	((sizeof(x) / sizeof(0 [x])) /			\
	 ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

#define container_of(ptr, type, member)                              \
	__extension__({                                              \
		__typeof__(((type *) 0)->member) *__mptr = (ptr);    \
		(type *) ((char *) __mptr - offsetof(type, member)); \
	})

#define const_container_of(ptr, type, member)                           \
	__extension__({                                                 \
		const __typeof__(((type *) 0)->member) *__mptr = (ptr); \
		(const type *) ((const char *) __mptr -                 \
				offsetof(type, member));                \
	})

// The "inline" keyword alone takes effect only when the optimization level
// is high enough.  Define INLINE to force the gcc to "always inline".
#define INLINE __attribute__((always_inline)) inline

#define __always_unused __attribute__((unused))
#define __maybe_unused  __attribute__((unused))
#define __must_check    __attribute__((warn_unused_result))
#define noinline        __attribute__((__noinline__))
#define __packed        __attribute__((packed))
#define __printf(a, b)  __attribute__((__format__(printf, a, b)))

/**
 * CPU Branch-prediction hints, courtesy of GCC. Defining these as inline
 * functions instead of macros spoils their magic, sadly.
 **/
#define likely(expr) __builtin_expect(!!(expr), 1)
#define unlikely(expr) __builtin_expect(!!(expr), 0)


#if __has_attribute(__fallthrough__)
#define fallthrough	__attribute__((__fallthrough__))
#else
#define fallthrough	do {} while (0)  /* fallthrough */
#endif

#endif /* COMMON_COMPILER_H */
