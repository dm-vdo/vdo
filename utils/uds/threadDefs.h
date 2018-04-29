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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/threadDefs.h#1 $
 *
 * LINUX USER-SPACE VERSION
 */

#ifndef LINUX_USER_THREAD_DEFS_H
#define LINUX_USER_THREAD_DEFS_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#ifndef NDEBUG
#define MUTEX_INITIALIZER PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#else
#define MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

extern const bool DO_ASSERTIONS;

typedef pthread_barrier_t  Barrier;
typedef pthread_cond_t     CondVar;
typedef pthread_mutex_t    Mutex;
typedef sem_t              Semaphore;
typedef pthread_t          Thread;
typedef pid_t              ThreadId;

/**
 * Initialize a condition variable attributes object.
 *
 * @param cond_attr condition variable attributes object to init
 *
 * @return          UDS_SUCCESS or error code
 **/
int initCondAttr(pthread_condattr_t *cond_attr);

/**
 * Set the "clock" condition variable attribute.
 *
 * @param cond_attr condition variable attributes object
 * @param clockID   the clock to use
 *
 * @return          UDS_SUCCESS or error code
 **/
int setClockCondAttr(pthread_condattr_t *cond_attr, clockid_t clockID);

/**
 * Destroy a condition variable attributes object.
 *
 * @param cond_attr condition variable attributes object to destroy
 *
 * @return          UDS_SUCCESS or error code
 **/
int destroyCondAttr(pthread_condattr_t *cond_attr);

/**
 * Initialize a condition variable.
 *
 * @param cond       condition variable to init
 * @param cond_attr  condition variable attributes
 *
 * @return           UDS_SUCCESS or error code
 **/
int initCondWithAttr(CondVar *cond, pthread_condattr_t *cond_attr);

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
int initMutex(Mutex *mutex);

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
 * Retrieve the affinity mask.
 * (see also sched_getaffinity)
 *
 * @param pid         pid of process for which to get the affinity mask or 0
 *                    for the current process
 * @param cpusetsize  length in bytes of the mask
 * @param mask        pointer to a CPU set
 *
 * @return            UDS_SUCCESS or error code
 **/
int schedGetAffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);

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
 * Destroy a thread attribute object.
 *
 * @param attr  a pointer to the attribute object to destroy
 *
 * @return      UDS_SUCCESS or error code
 **/
int destroyThreadAttr(pthread_attr_t *attr);

/**
 * init a thread attribute object.
 *
 * @param attr  a pointer to the attribute object
 *
 * @return      UDS_SUCCESS or error code
 **/
int initThreadAttr(pthread_attr_t *attr);

/**
 * Add information about the thread's minimum stack size to the thread
 * attribute object.
 *
 * @param attr       attribute object in which to set the stack size.
 * @param stacksize  the desired minimum stack size
 *
 * @return           UDS_SUCCESS or error code
 **/
int setThreadStackSize(pthread_attr_t *attr, size_t stacksize);

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
