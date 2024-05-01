/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

#include <linux/atomic.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdbool.h>

#include "errors.h"
#include "time-utils.h"

/* Thread and synchronization utilities */

struct mutex {
	pthread_mutex_t mutex;
};

struct semaphore {
	sem_t semaphore;
};

struct thread {
	pthread_t thread;
};

struct threads_barrier {
	pthread_barrier_t barrier;
};

#ifndef NDEBUG
#define UDS_MUTEX_INITIALIZER { .mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP }
#else
#define UDS_MUTEX_INITIALIZER { .mutex = PTHREAD_MUTEX_INITIALIZER }
#endif

extern const bool UDS_DO_ASSERTIONS;

unsigned int num_online_cpus(void);
pid_t __must_check uds_get_thread_id(void);

void vdo_perform_once(atomic_t *once_state, void (*function) (void));

int __must_check vdo_create_thread(void (*thread_function)(void *), void *thread_data,
				   const char *name, struct thread **new_thread);
void vdo_join_threads(struct thread *thread);

void uds_get_thread_name(char *name);

static inline void cond_resched(void)
{
	/*
	 * On Linux sched_yield always succeeds so the result can be
	 * safely ignored.
	 */
	(void) sched_yield();
}

int uds_initialize_mutex(struct mutex *mutex, bool assert_on_error);
int __must_check uds_init_mutex(struct mutex *mutex);
int uds_destroy_mutex(struct mutex *mutex);
void uds_lock_mutex(struct mutex *mutex);
void uds_unlock_mutex(struct mutex *mutex);

void initialize_threads_barrier(struct threads_barrier *barrier,
				unsigned int thread_count);
void destroy_threads_barrier(struct threads_barrier *barrier);
void enter_threads_barrier(struct threads_barrier *barrier);

int __must_check uds_initialize_semaphore(struct semaphore *semaphore,
					  unsigned int value);
int uds_destroy_semaphore(struct semaphore *semaphore);
void uds_acquire_semaphore(struct semaphore *semaphore);
bool __must_check uds_attempt_semaphore(struct semaphore *semaphore, ktime_t timeout);
void uds_release_semaphore(struct semaphore *semaphore);

#endif /* UDS_THREADS_H */
