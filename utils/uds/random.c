// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright Red Hat
 */

#include "random.h"

#include "permassert.h"

unsigned int random_in_range(unsigned int lo, unsigned int hi)
{
	return lo + random() % (hi - lo + 1);
}

void random_compile_time_assertions(void)
{
	STATIC_ASSERT((((uint64_t) RAND_MAX + 1) & RAND_MAX) == 0);
}

void fill_randomly(void *ptr, size_t len)
{
	uint64_t rand_num = 0;
	uint64_t rand_mask = 0;
	const uint64_t multiplier = (uint64_t) RAND_MAX + 1;

	byte *bp = ptr;

	for (size_t i = 0; i < len; ++i) {
		if (rand_mask < 0xff) {
			rand_num = rand_num * multiplier + random();
			rand_mask = rand_mask * multiplier + RAND_MAX;
		}
		bp[i] = rand_num & 0xff;
		rand_num >>= 8;
		rand_mask >>= 8;
	}
}
