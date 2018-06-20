/*
 * Copyright (c) 2018 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/flanders/userLinux/uds/atomicDefs.h#2 $
 */

#ifndef LINUX_USER_ATOMIC_DEFS_H
#define LINUX_USER_ATOMIC_DEFS_H

#include "compiler.h"
#include "typeDefs.h"

// The atomic interfaces are chosen to exactly match those interfaces defined
// by the Linux kernel.  This is the matching user-mode implementation.

typedef struct {
  int32_t value;
} atomic_t;

typedef struct {
  int64_t value;
} atomic64_t;

#define ATOMIC_INIT(i)  { (i) }

/*
 * Prevent the compiler from merging or refetching accesses.  The compiler is
 * also forbidden from reordering successive instances of ACCESS_ONCE(), but
 * only when the compiler is aware of some particular ordering.  One way to
 * make the compiler aware of ordering is to put the two invocations of
 * ACCESS_ONCE() in different C statements.
 */
#define ACCESS_ONCE(x) (*(volatile __typeof__(x) *)&(x))

/*****************************************************************************
 * Beginning of the barrier methods.
 *****************************************************************************/

/**
 * Stop GCC from moving memory operations across a point in the instruction
 * stream.  This is how the kernel uses this method.
 *
 * We also need this method in association with the __sync builtins, because
 * (at least, as of GCC versions 4.6 and earlier on x86_64) the __sync
 * operations don't actually act as the barriers the compiler documentation
 * says they should.
 **/
static INLINE void barrier(void)
{
  /*
   * asm volatile cannot be removed, and the memory clobber tells the
   * compiler not to move memory accesses past the asm.  We don't
   * actually need any instructions issued on x86_64, as synchronizing
   * instructions are ordered with respect to both loads and stores,
   * with some irrelevant-to-us exceptions.
   */
  __asm__ __volatile__("" : : : "memory");
}

/**
 * Provide a memory barrier.
 *
 * Generate a full memory fence for the compiler and CPU. Load and store
 * operations issued before the fence will not be re-ordered with operations
 * issued after the fence.
 **/
static INLINE void smp_mb(void)
{
  /*
   * X86 full fence. Supposedly __sync_synchronize() will do this, but
   * either the GCC documentation is a lie or GCC is broken.
   *
   * XXX http://blogs.sun.com/dave/entry/atomic_fetch_and_add_vs says
   * atomicAdd of zero may be a better way to spell this on current CPUs.
   */
  __asm__ __volatile__("mfence" : : : "memory");
}

/**
 * Provide a read memory barrier.
 *
 * Memory load operations that precede this fence will be prevented from
 * changing order with any that follow this fence, by either the compiler or
 * the CPU. This can be used to ensure that the load operations accessing the
 * fields of a structure are not re-ordered so they actually take effect before
 * a pointer to the structure is resolved.
 **/
static INLINE void smp_rmb(void)
{
  // XXX The implementation on x86 is more aggressive than necessary.
  __asm__ __volatile__("lfence" : : : "memory");
}

/**
 * Provide a write memory barrier.
 *
 * Memory store operations that precede this fence will be prevented from
 * changing order with any that follow this fence, by either the compiler or
 * the CPU. This can be used to ensure that the store operations initializing
 * the fields of a structure are not re-ordered so they actually take effect
 * after a pointer to the structure is published.
 **/
static INLINE void smp_wmb(void)
{
  // XXX The implementation on x86 is more aggressive than necessary.
  __asm__ __volatile__("sfence" : : : "memory");
}

/**
 * Provide a memory barrier before an atomic operation.
 **/
static INLINE void smp_mb__before_atomic(void)
{
  smp_mb();
}

/**
 * Provide a read barrier, if needed, between dependent reads.
 *
 * On most architectures, a read issued using a memory location that was
 * itself read from memory (or derived from something read from memory) cannot
 * pick up "stale" data if the data was written out before the pointer itself
 * was saved and proper write fencing was used. On one or two, like the Alpha,
 * a barrier is needed between the reads to ensure that proper cache
 * invalidation happens.
 **/
static INLINE void smp_read_barrier_depends(void)
{
  // Nothing needed on x86 in user mode.
}

/*****************************************************************************
 * Beginning of the 32 bit atomic support.
 *****************************************************************************/

/**
 * Add a signed int to a 32-bit atomic variable.  The addition is atomic, but
 * there are no memory barriers implied by this method.
 *
 * @param delta  the value to be added to (or subtracted from) the variable
 * @param atom   a pointer to the atomic variable
 **/
static INLINE void atomic_add(int delta, atomic_t *atom)
{
  /*
   * According to the kernel documentation, the addition is atomic, but there
   * are no memory barriers implied by this method.
   *
   * The x86 implementation does do memory barriers.
   */
  __sync_add_and_fetch(&atom->value, delta);
}

/**
 * Add a signed int to a 32-bit atomic variable.  The addition is properly
 * atomic, and there are memory barriers.
 *
 * @param atom   a pointer to the atomic variable
 * @param delta  the value to be added (or subtracted) from the variable
 *
 * @return the new value of the atom after the add operation
 **/
static INLINE int atomic_add_return(int delta, atomic_t *atom)
{
  barrier();
  int result = __sync_add_and_fetch(&atom->value, delta);
  barrier();
  return result;
}

/**
 * Compare and exchange a 32-bit atomic variable.  The operation is properly
 * atomic and performs a memory barrier.
 *
 * @param atom  a pointer to the atomic variable
 * @param old   the value that must be present to perform the swap
 * @param new   the value to be swapped for the required value
 *
 * @return the old value
 **/
static INLINE int atomic_cmpxchg(atomic_t *atom, int old, int new)
{
  return __sync_val_compare_and_swap(&atom->value, old, new);
}

/**
 * Increment a 32-bit atomic variable, without any memory barriers.
 *
 * @param atom   a pointer to the atomic variable
 **/
static INLINE void atomic_inc(atomic_t *atom)
{
  /*
   * According to the kernel documentation, the addition is atomic, but there
   * are no memory barriers implied by this method.
   *
   * The x86 implementation does do memory barriers.
   */
  __sync_add_and_fetch(&atom->value, 1);
}

/**
 * Read a 32-bit atomic variable, without any memory barriers.
 *
 * @param atom   a pointer to the atomic variable
 **/
static INLINE int atomic_read(const atomic_t *atom)
{
  return ACCESS_ONCE(atom->value);
}

/**
 * Read a 32-bit atomic variable, with an acquire memory barrier.
 *
 * @param atom  a pointer to the atomic variable
 **/
static INLINE int atomic_read_acquire(const atomic_t *atom)
{
  int value = ACCESS_ONCE(atom->value);
  smp_mb();
  return value;
}

/**
 * Set a 32-bit atomic variable, without any memory barriers.
 *
 * @param atom   a pointer to the atomic variable
 * @param value  the value to set it to
 **/
static INLINE void atomic_set(atomic_t *atom, int value)
{
  atom->value = value;
}

/**
 * Set a 32-bit atomic variable, with a release memory barrier.
 *
 * @param atom   a pointer to the atomic variable
 * @param value  the value to set it to
**/
static INLINE void atomic_set_release(atomic_t *atom, int value)
{
  smp_mb();
  atomic_set(atom, value);
}

/*****************************************************************************
 * Beginning of the 64 bit atomic support.
 *****************************************************************************/

/**
 * Add a signed long to a 64-bit atomic variable.  The addition is atomic, but
 * there are no memory barriers implied by this method.
 *
 * @param delta  the value to be added to (or subtracted from) the variable
 * @param atom   a pointer to the atomic variable
 **/
static INLINE void atomic64_add(long delta, atomic64_t *atom)
{
  /*
   * According to the kernel documentation, the addition is atomic, but there
   * are no memory barriers implied by this method.
   *
   * The x86 implementation does do memory barriers.
   */
  __sync_add_and_fetch(&atom->value, delta);
}

/**
 * Add a signed long to a 64-bit atomic variable.  The addition is properly
 * atomic, and there are memory barriers.
 *
 * @param atom   a pointer to the atomic variable
 * @param delta  the value to be added (or subtracted) from the variable
 *
 * @return the new value of the atom after the add operation
 **/
static INLINE long atomic64_add_return(long delta, atomic64_t *atom)
{
  barrier();
  long result = __sync_add_and_fetch(&atom->value, delta);
  barrier();
  return result;
}

/**
 * Compare and exchange a 64-bit atomic variable.  The operation is properly
 * atomic and performs a memory barrier.
 *
 * @param atom  a pointer to the atomic variable
 * @param old   the value that must be present to perform the swap
 * @param new   the value to be swapped for the required value
 *
 * @return the old value
 **/
static INLINE long atomic64_cmpxchg(atomic64_t *atom, long old, long new)
{
  return __sync_val_compare_and_swap(&atom->value, old, new);
}

/**
 * Increment a 64-bit atomic variable, without any memory barriers.
 *
 * @param atom   a pointer to the atomic variable
 **/
static INLINE void atomic64_inc(atomic64_t *atom)
{
  /*
   * According to the kernel documentation, the addition is atomic, but there
   * are no memory barriers implied by this method.
   *
   * The x86 implementation does do memory barriers.
   */
  __sync_add_and_fetch(&atom->value, 1);
}

/**
 * Read a 64-bit atomic variable, without any memory barriers.
 *
 * @param atom   a pointer to the atomic variable
 **/
static INLINE long atomic64_read(const atomic64_t *atom)
{
  return ACCESS_ONCE(atom->value);
}

/**
 * Read a 64-bit atomic variable, with an acquire memory barrier.
 *
 * @param atom  a pointer to the atomic variable
 **/
static INLINE long atomic64_read_acquire(const atomic64_t *atom)
{
  long value = ACCESS_ONCE(atom->value);
  smp_mb();
  return value;
}

/**
 * Set a 64-bit atomic variable, without any memory barriers.
 *
 * @param atom   a pointer to the atomic variable
 * @param value  the value to set it to
 **/
static INLINE void atomic64_set(atomic64_t *atom, long value)
{
  atom->value = value;
}

/**
 * Set a 64-bit atomic variable, with a release memory barrier.
 *
 * @param atom   a pointer to the atomic variable
 * @param value  the value to set it to
**/
static INLINE void atomic64_set_release(atomic64_t *atom, long value)
{
  smp_mb();
  atomic64_set(atom, value);
}

/*****************************************************************************
 * Generic exchange support.
 *****************************************************************************/

/*
 * Exchange a location's value atomically, with a full memory barrier.
 *
 * The location is NOT an "atomic*_t" type, but any primitive type for which
 * an exchange can be done atomically. (This varies by processor, but
 * generally a word-sized or pointer-sized value is supported.) As this uses a
 * type-generic compiler interface, it must be implemented as a macro.
 *
 * @param PTR     a pointer to the location to be updated
 * @param NEWVAL  the new value to be stored
 *
 * @return the old value
 */
#define xchg(PTR,NEWVAL)                                                \
  __extension__                                                         \
  ({                                                                    \
    __typeof__(*(PTR)) __xchg_result;                                   \
    barrier(); /* paranoia, for old gcc bugs */                         \
    __xchg_result = __atomic_exchange_n((PTR), (NEWVAL), __ATOMIC_SEQ_CST); \
    barrier(); /* more paranoia */                                      \
    __xchg_result;                                                      \
  })

#endif /* LINUX_USER_ATOMIC_DEFS_H */
