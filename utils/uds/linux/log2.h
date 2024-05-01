/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef _LINUX_LOG2_H
#define _LINUX_LOG2_H

#include "permassert.h"

/* Compute the number of bits to represent n */
static inline unsigned int bits_per(unsigned int n)
{
	unsigned int bits = 1;

	while (n > 1) {
		n >>= 1;
		bits++;
	}

	return bits;
}

/**
 * is_power_of_2() - Return true if and only if a number is a power of two.
 */
static inline bool is_power_of_2(uint64_t n)
{
	return (n > 0) && ((n & (n - 1)) == 0);
}

/**
 * ilog2() - Efficiently calculate the base-2 logarithm of a number truncated
 *           to an integer value.
 * @n: The input value.
 *
 * This also happens to be the bit index of the highest-order non-zero bit in
 * the binary representation of the number, which can easily be used to
 * calculate the bit shift corresponding to a bit mask or an array capacity,
 * or to calculate the binary floor or ceiling (next lowest or highest power
 * of two).
 *
 * Return: The integer log2 of the value, or -1 if the value is zero.
 */
static inline int ilog2(uint64_t n)
{
	VDO_ASSERT_LOG_ONLY(n != 0, "ilog2() may not be passed 0");
	/*
	 * Many CPUs, including x86, directly support this calculation, so use
	 * the GCC function for counting the number of leading high-order zero
	 * bits.
	 */
	return 63 - __builtin_clzll(n);
}

#endif /* _LINUX_LOG2_H */
