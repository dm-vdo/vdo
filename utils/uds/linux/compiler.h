/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef LINUX_COMPILER_H
#define LINUX_COMPILER_H

#include <linux/compiler_attributes.h>

/*
 * CPU Branch-prediction hints, courtesy of GCC. Defining these as inline functions instead of
 * macros spoils their magic, sadly.
 */
#define likely(expr) __builtin_expect(!!(expr), 1)
#define unlikely(expr) __builtin_expect(!!(expr), 0)

/*
 * Count the elements in a static array while attempting to catch some type errors. (See
 * http://stackoverflow.com/a/1598827 for an explanation.)
 */
#define ARRAY_SIZE(x) ((sizeof(x) / sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

/* Defined in linux/container_of.h */
#define container_of(ptr, type, member)                              \
	__extension__({                                              \
		__typeof__(((type *) 0)->member) * __mptr = (ptr);   \
		(type *) ((char *) __mptr - offsetof(type, member)); \
	})

#endif /* LINUX_COMPILER_H */
