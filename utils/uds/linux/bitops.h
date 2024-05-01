/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * These are the small parts of linux/bits.h that we actually require for
 * unit testing, reimplemented without all of the architecture specific
 * macros.
 *
 * Copyright 2023 Red Hat
 *
 */

#ifndef _TOOLS_LINUX_BITOPS_H_
#define _TOOLS_LINUX_BITOPS_H_

#include <linux/bits.h> 
#include <linux/compiler.h> 
#include <linux/const.h>

// From vdso/const.h
#define UL(x)		(_UL(x))
#define ULL(x)		(_ULL(x))

#define BITS_PER_LONG 64
#define BIT_MASK(nr) (UL(1) << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))

#define BITS_PER_TYPE(type)	(sizeof(type) * BITS_PER_BYTE)
#define BITS_TO_LONGS(nr)	__KERNEL_DIV_ROUND_UP(nr, BITS_PER_TYPE(long))
#define BITS_TO_U64(nr)		__KERNEL_DIV_ROUND_UP(nr, BITS_PER_TYPE(u64))
#define BITS_TO_U32(nr)		__KERNEL_DIV_ROUND_UP(nr, BITS_PER_TYPE(u32))
#define BITS_TO_BYTES(nr)	__KERNEL_DIV_ROUND_UP(nr, BITS_PER_TYPE(char))

/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 **/
static inline void __set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);

	addr[BIT_WORD(nr)] |= mask;
}

/**********************************************************************/
static inline void __clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);

	addr[BIT_WORD(nr)] &= ~mask;
}

/**
 * test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 **/
static inline int test_bit(int nr, const volatile unsigned long *addr)
{
	return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
}

/**********************************************************************/
unsigned long __must_check
find_next_zero_bit(const unsigned long *addr,
		   unsigned long size,
		   unsigned long offset);

/**********************************************************************/
unsigned long __must_check
find_first_zero_bit(const unsigned long *addr,
		    unsigned long size);

#endif /* _TOOLS_LINUX_BITOPS_H_ */

