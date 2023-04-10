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
 * $Id: //eng/uds-releases/jasper/src/uds/random.h#3 $
 */

#ifndef RANDOM_H
#define RANDOM_H

#ifdef __KERNEL__
#include <linux/random.h>
#else
#include <stdlib.h>
#endif

#include "compiler.h"
#include "typeDefs.h"

/**
 * Get random unsigned integer in a given range
 *
 * @param lo  Minimum unsigned integer value
 * @param hi  Maximum unsigned integer value
 *
 * @return unsigned integer in the interval [lo,hi]
 **/
unsigned int randomInRange(unsigned int lo, unsigned int hi);

/**
 * Special function wrapper required for compile-time assertions. This
 * function will fail to compile if RAND_MAX is not of the form 2^n - 1.
 **/
void randomCompileTimeAssertions(void);

/**
 * Fill bytes with random data.
 *
 * @param ptr   where to store bytes
 * @param len   number of bytes to write
 **/
#ifdef __KERNEL__
static INLINE void fillRandomly(void *ptr, size_t len)
{
  get_random_bytes(ptr, len);
}
#else
void fillRandomly(void *ptr, size_t len);
#endif

#ifdef __KERNEL__
#define RAND_MAX 2147483647

/**
 * Random number generator
 *
 * @return a random number in the rand 0 to RAND_MAX
 **/
static INLINE long random(void)
{
  long value;
  fillRandomly(&value, sizeof(value));
  return value & RAND_MAX;
}
#endif

#endif /* RANDOM_H */
