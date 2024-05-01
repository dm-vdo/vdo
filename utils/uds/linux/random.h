/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef LINUX_RANDOM_H
#define LINUX_RANDOM_H

#include <stddef.h>

void get_random_bytes(void *buffer, size_t byte_count);

#endif /* LINUX_RANDOM_H */
