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
 */

#ifndef UDS_THREADS_H
#define UDS_THREADS_H

#include "compiler.h"
#include "errors.h"
#include "threadOnce.h"
#include "timeUtils.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

struct cond_var {
	pthread_cond_t condition;
};

struct mutex {
	pthread_mutex_t mutex;
};

struct semaphore {
	sem_t semaphore;
};

struct thread {
	pthread_t thread;
};

struct barrier {
	pthread_barrier_t barrier;
};

#ifndef NDEBUG
#define UDS_MUTEX_INITIALIZER PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#else
#define UDS_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#endif

extern const bool UDS_DO_ASSERTIONS;


/**
 * Create a thread, logging any cause of failure.
 *
 * @param thread_func  function to run in new thread
 * @param thread_data  private data for new thread
 * @param name         name of the new thread
 * @param new_thread   where to store the new thread id
 *
 * @return       success or failure indication
 **/
int __must_check uds_create_thread(void (*thread_func)(void *),
				   void *thread_data,
				   const char *name,
				   struct thread **new_thread);

/**
 * Retrieve the current numbers of cores.
 *
 * This is either the total number or the number of cores that this
 * process has been limited to.
 *
 * @return      number of cores
 **/
unsigned int uds_get_num_cores(void);

/**
 * Return the id of the current thread.
 *
 * @return the thread id
 **/
pid_t __must_check uds_get_thread_id(void);

/**
 * Get the name of the current thread.
 *
 * @param name   a buffer of size at least 16 to write the name to
 **/
void uds_get_thread_name(char *name);

/**
 * Wait for termination of another thread.
 *
 *
 * @param th             The thread for which to wait.
 *
 * @return               UDS_SUCCESS or error code
 **/
int uds_join_threads(struct thread *th);


/**
 * Initialize a thread synchronization barrier (also known as a rendezvous).
 *
 * @param barrier       the barrier to initialize
 * @param thread_count  the number of threads that must enter the barrier
 *                      before any threads are permitted to leave it
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check uds_initialize_barrier(struct barrier *barrier,
					unsigned int thread_count);

/**
 * Destroy a thread synchronization barrier.
 *
 * @param barrier   the barrier to destroy
 *
 * @return UDS_SUCCESS or an error code
 **/
int uds_destroy_barrier(struct barrier *barrier);

/**
 * Enter a thread synchronization barrier, waiting for the configured number
 * of threads to have entered before exiting the barrier. Exactly one thread
 * will be arbitrarily selected to be flagged as the "winner" of a barrier.
 *
 * @param barrier   the barrier to enter
 * @param winner    if non-NULL, a pointer to the flag indicating whether the
 *                  calling thread was the unique winner
 *
 * @return UDS_SUCCESS or an error code
 **/
int uds_enter_barrier(struct barrier *barrier, bool *winner);

/**
 * Initialize a condition variable with default attributes.
 *
 * @param cond       condition variable to init
 *
 * @return           UDS_SUCCESS or error code
 **/
int __must_check uds_init_cond(struct cond_var *cond);

/**
 * Signal a condition variable.
 *
 * @param cond  condition variable to signal
 *
 * @return      UDS_SUCCESS or error code
 **/
int uds_signal_cond(struct cond_var *cond);

/**
 * Broadcast a condition variable.
 *
 * @param cond  condition variable to broadcast
 *
 * @return      UDS_SUCCESS or error code
 **/
int uds_broadcast_cond(struct cond_var *cond);

/**
 * Wait on a condition variable.
 *
 * @param cond    condition variable to wait on
 * @param mutex   mutex to release while waiting
 *
 * @return        UDS_SUCCESS or error code
 **/
int uds_wait_cond(struct cond_var *cond, struct mutex *mutex);

/**
 * Wait on a condition variable with a timeout.
 *
 * @param cond     condition variable to wait on
 * @param mutex    mutex to release while waiting
 * @param timeout  the relative time until the timeout expires
 *
 * @return error code (ETIMEDOUT if the deadline is hit)
 **/
int uds_timed_wait_cond(struct cond_var *cond,
			struct mutex *mutex,
			ktime_t timeout);

/**
 * Destroy a condition variable.
 *
 * @param cond  condition variable to destroy
 *
 * @return      UDS_SUCCESS or error code
 **/
int uds_destroy_cond(struct cond_var *cond);

/**
 * Initialize a mutex, optionally asserting if the mutex initialization fails.
 * This function should only be called directly in places where making
 * assertions is not safe.
 *
 * @param mutex            the mutex to initialize
 * @param assert_on_error  if <code>true</code>, an error initializing the
 *                         mutex will make an assertion
 *
 * @return UDS_SUCCESS or an error code
 **/
int uds_initialize_mutex(struct mutex *mutex, bool assert_on_error);

/**
 * Initialize the default type (error-checking during development) mutex.
 *
 * @param mutex the mutex to initialize
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check uds_init_mutex(struct mutex *mutex);

/**
 * Destroy a mutex (with error checking during development).
 *
 * @param mutex mutex to destroy
 *
 * @return UDS_SUCCESS or error code
 **/
int uds_destroy_mutex(struct mutex *mutex);

/**
 * Lock a mutex, with optional error checking during development.
 *
 * @param mutex mutex to lock
 **/
void uds_lock_mutex(struct mutex *mutex);

/**
 * Unlock a mutex, with optional error checking during development.
 *
 * @param mutex mutex to unlock
 **/
void uds_unlock_mutex(struct mutex *mutex);

/**
 * Initialize a semaphore used among threads in the same process.
 *
 * @param semaphore the semaphore to initialize
 * @param value     the initial value of the semaphore
 *
 * @return UDS_SUCCESS or an error code
 **/
int __must_check uds_initialize_semaphore(struct semaphore *semaphore,
					  unsigned int value);

/**
 * Destroy a semaphore used among threads in the same process.
 *
 * @param semaphore the semaphore to destroy
 *
 * @return UDS_SUCCESS or an error code
 **/
int uds_destroy_semaphore(struct semaphore *semaphore);

/**
 * Acquire a permit from a semaphore, waiting if none are currently available.
 *
 * @param semaphore the semaphore to acquire
 **/
void uds_acquire_semaphore(struct semaphore *semaphore);

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
bool __must_check uds_attempt_semaphore(struct semaphore *semaphore,
					ktime_t timeout);

/**
 * Release a semaphore, incrementing the number of available permits.
 *
 * @param semaphore the semaphore to increment
 **/
void uds_release_semaphore(struct semaphore *semaphore);

/**
 * Yield the time slice in the given thread.
 *
 * @return UDS_SUCCESS or an error code
 **/
int uds_yield_scheduler(void);

/**
 * Allocate a thread specific key for thread specific data.
 *
 * @param key            points to location for new key
 * @param destr_function destructor function called when thread exits
 *
 * @return               UDS_SUCCESS or error code
 **/
int uds_create_thread_key(pthread_key_t *key, void (*destr_function)(void *));

/**
 * Delete a thread specific key for thread specific data.
 *
 * @param key  key to delete
 *
 * @return     UDS_SUCCESS or error code
 **/
int uds_delete_thread_key(pthread_key_t key);

/**
 * Set pointer for thread specific data.
 *
 * @param key      key to be associated with pointer
 * @param pointer  data associated with key
 *
 * @return         UDS_SUCCESS or error code
 **/
int uds_set_thread_specific(pthread_key_t key, const void *pointer);

/**
 * Get pointer for thread specific data.
 *
 * @param key  key identifying the thread specific data
 **/
void *uds_get_thread_specific(pthread_key_t key);

#endif /* UDS_THREADS_H */
