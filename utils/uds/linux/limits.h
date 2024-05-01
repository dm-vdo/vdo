/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef UDS_LINUX_LIMITS_H
#define UDS_LINUX_LIMITS_H

#include <linux/types.h>
#include <limits.h>

#define U8_MAX  ((u8)~0ul)
#define S8_MAX  ((s8)(U8_MAX >> 1))
#define U16_MAX ((u16)~0ul)
#define S16_MAX ((s16)(U16_MAX >> 1))
#define U32_MAX ((u32)~0ul)
#define S32_MAX ((s32)(U32_MAX >> 1))
#define U64_MAX ((u64)~0ul)
#define S64_MAX ((s64)(U64_MAX >> 1))

/*
 * NAME_MAX and PATH_MAX were copied from /usr/include/limits/linux.h.
 */
#define NAME_MAX  255
#define PATH_MAX  4096

#endif /* UDS_LINUX_LIMITS_H */
