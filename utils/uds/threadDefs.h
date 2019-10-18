/*
 * Copyright (c) 2019 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/jasper/userLinux/uds/threadDefs.h#2 $
 *
 * LINUX USER-SPACE VERSION
 */

#ifndef LINUX_USER_THREAD_DEFS_H
#define LINUX_USER_THREAD_DEFS_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#include "timeUtils.h"

#ifndef NDEBUG
#define MUTEX_INITIALIZER PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#else
#define MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

extern const bool DO_ASSERTIONS;

typedef pthread_barrier_t Barrier;
typedef pthread_cond_t    CondVar;
typedef pthread_mutex_t   Mutex;
typedef sem_t             Semaphore;
typedef pthread_t         Thread;
typedef pid_t             ThreadId;

/**
 * Initialize a mutex, optionally asserting if the mutex initialization fails.
 * This function should only be called directly in places where making
 * assertions is not safe.
 *
 * @param mutex         the mutex to initialize
 * @param assertOnError if <code>true</code>, an error initializing the
 *                      mutex will make an assertion
 *
 * @return UDS_SUCCESS or an error code
 **/
int initializeMutex(Mutex *mutex, bool assertOnError);

/**
 * Initialize the default type (error-checking during development) mutex.
 *
 * @param mutex the mutex to initialize
 *
 * @return UDS_SUCCESS or an error code
 **/
int initMutex(Mutex *mutex) __attribute__((warn_unused_result));

/**
 * Destroy a mutex (with error checking during development).
 *
 * @param mutex mutex to destroy
 *
 * @return UDS_SUCCESS or error code
 **/
int destroyMutex(Mutex *mutex);

/**
 * Lock a mutex, with optional error checking during development.
 *
 * @param mutex mutex to lock
 **/
void lockMutex(Mutex *mutex);

/**
 * Unlock a mutex, with optional error checking during development.
 *
 * @param mutex mutex to unlock
 **/
void unlockMutex(Mutex *mutex);

/**
 * Initialize a semaphore used among threads in the same process.
 *
 * @param semaphore the semaphore to initialize
 * @param value     the initial value of the semaphore
 *
 * @return UDS_SUCCESS or an error code
 **/
int initializeSemaphore(Semaphore *semaphore, unsigned int value)
  __attribute__((warn_unused_result));

/**
 * Destroy a semaphore used among threads in the same process.
 *
 * @param semaphore the semaphore to destroy
 *
 * @return UDS_SUCCESS or an error code
 **/
int destroySemaphore(Semaphore *semaphore);

/**
 * Acquire a permit from a semaphore, waiting if none are currently available.
 *
 * @param semaphore the semaphore to acquire
 **/
void acquireSemaphore(Semaphore *semaphore);

/**
 * Attempt to acquire a permit from a semaphore.
 *
 * If a permit is available, it is claimed and the function immediately
 * returns true. If a timeout is zero or negative, the function immediately
 * returns false. Otherwise, this will wait either a permit to become
 * available (returning true) or the relative timeout to expire (returning
 * false).
 *
 * @param semaphore the semaphore to decrement
 * @param timeout   the relative time until the timeout expires
 *
 * @return true if a permit was acquired, otherwise false
 **/
bool attemptSemaphore(Semaphore *semaphore, RelTime timeout)
  __attribute__((warn_unused_result));

/**
 * Release a semaphore, incrementing the number of available permits.
 *
 * @param semaphore the semaphore to increment
 **/
void releaseSemaphore(Semaphore *semaphore);

/**
 * Get a value identifying the CPU on which this thread is running (see
 * sched_getcpu).
 *
 * The return value is just a hint; nothing prevents the scheduler from
 * migrating the thread to another CPU after just this call returns. This is
 * not currently supported when running under Valgrind, in which case a small
 * modulus of the hash of the current thread identifier is returned instead.
 *
 * @return a non-negative CPU number or a small pseudo-random value
 **/
unsigned int getScheduledCPU(void);

/**
 * Count the total number of CPU cores on the host. This value may be
 * larger the total number of cores available for threads to use when
 * scheduler affinity is used to constrain the set of CPUs.
 *
 * @return the total number of CPUs
 **/
unsigned int countAllCores(void);

/**
 * Get the name of the current thread.
 *
 * @param name   a buffer of size at least 16 to write the name to
 **/
void getThreadName(char *name);

/**
 * Allocate a thread specific key for thread specific data.
 *
 * @param key            points to location for new key
 * @param destr_function destructor function called when thread exits
 *
 * @return               UDS_SUCCESS or error code
 **/
int createThreadKey(pthread_key_t *key, void (*destr_function) (void *));

/**
 * Delete a thread specific key for thread specific data.
 *
 * @param key  key to delete
 *
 * @return     UDS_SUCCESS or error code
 **/
int deleteThreadKey(pthread_key_t key);

/**
 * Set pointer for thread specific data.
 *
 * @param key      key to be associated with pointer
 * @param pointer  data associated with key
 *
 * @return         UDS_SUCCESS or error code
 **/
int setThreadSpecific(pthread_key_t key, const void *pointer);

/**
 * Get pointer for thread specific data.
 *
 * @param key  key identifying the thread specific data
 **/
void *getThreadSpecific(pthread_key_t key);

#endif // LINUX_USER_THREAD_DEFS_H
