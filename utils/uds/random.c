// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include <linux/random.h>
#include <linux/types.h>

#include "random.h"

void get_random_bytes(void *buffer, size_t byte_count)
{
	uint64_t rand_num = 0;
	uint64_t rand_mask = 0;
	const uint64_t multiplier = (uint64_t) RAND_MAX + 1;
	u8 *data = buffer;
	size_t i;

	for (i = 0; i < byte_count; i++) {
		if (rand_mask < 0xff) {
			rand_num = rand_num * multiplier + random();
			rand_mask = rand_mask * multiplier + RAND_MAX;
		}
		data[i] = rand_num & 0xff;
		rand_num >>= 8;
		rand_mask >>= 8;
	}
}
