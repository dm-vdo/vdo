/* SPDX-License-Identifier: GPL-2.0 */
/*
 * User space definitions for things in linux/cache.h
 *
 * Copyright 2023 Red Hat
 */

#ifndef __LINUX_CACHE_H
#define __LINUX_CACHE_H

#include <linux/compiler_attributes.h>

#if defined(__PPC__)
/* N.B.: Some PPC processors have smaller cache lines. */
#define L1_CACHE_BYTES 128
#elif defined(__s390x__)
#define L1_CACHE_BYTES 256
#elif defined(__x86_64__) || defined(__aarch64__) || defined(__riscv) || defined (__loongarch64)
#define L1_CACHE_BYTES 64
#else
#error "unknown cache line size"
#endif

#endif  /* __LINUX_CACHE_H */
