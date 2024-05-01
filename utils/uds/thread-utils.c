// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include "thread-utils.h"

#include <errno.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "logger.h"
#include "memory-alloc.h"
#include "permassert.h"
#include "syscalls.h"

enum {
	ONCE_NOT_DONE = 0,
	ONCE_IN_PROGRESS = 1,
	ONCE_COMPLETE = 2,
};

/**********************************************************************/
unsigned int num_online_cpus(void)
{
	cpu_set_t cpu_set;
	unsigned int n_cpus = 0;
	unsigned int i;

	if (sched_getaffinity(0, sizeof(cpu_set), &cpu_set) != 0) {
		vdo_log_warning_strerror(errno,
					 "sched_getaffinity() failed, using 1 as number of cores.");
		return 1;
	}

	for (i = 0; i < CPU_SETSIZE; i++)
		n_cpus += CPU_ISSET(i, &cpu_set);
	return n_cpus;
}

/**********************************************************************/
void uds_get_thread_name(char *name)
{
	process_control(PR_GET_NAME, (unsigned long) name, 0, 0, 0);
}

/**********************************************************************/
pid_t uds_get_thread_id(void)
{
	return syscall(SYS_gettid);
}

/* Run a function once only, and record that fact in the atomic value. */
void vdo_perform_once(atomic_t *once, void (*function)(void))
{
	for (;;) {
		switch (atomic_cmpxchg(once, ONCE_NOT_DONE, ONCE_IN_PROGRESS)) {
		case ONCE_NOT_DONE:
			function();
			atomic_set_release(once, ONCE_COMPLETE);
			return;
		case ONCE_IN_PROGRESS:
			sched_yield();
			break;
		case ONCE_COMPLETE:
			return;
		default:
			return;
		}
	}
}

struct thread_start_info {
	void (*thread_function)(void *);
	void *thread_data;
	const char *name;
};

/**********************************************************************/
static void *thread_starter(void *arg)
{
	struct thread_start_info *info = arg;
	void (*thread_function)(void *) = info->thread_function;
	void *thread_data = info->thread_data;

	/*
	 * The name is just advisory for humans examining it, so we don't
	 * care much if this fails.
	 */
	process_control(PR_SET_NAME, (unsigned long) info->name, 0, 0, 0);
	vdo_free(info);
	thread_function(thread_data);
	return NULL;
}

/**********************************************************************/
int vdo_create_thread(void (*thread_function)(void *),
		      void *thread_data,
		      const char *name,
		      struct thread **new_thread)
{
	int result;
	struct thread_start_info *info;
	struct thread *thread;

	result = vdo_allocate(1, struct thread_start_info, __func__, &info);
	if (result != VDO_SUCCESS)
		return result;
	info->thread_function = thread_function;
	info->thread_data = thread_data;
	info->name = name;

	result = vdo_allocate(1, struct thread, __func__, &thread);
	if (result != VDO_SUCCESS) {
		vdo_log_warning("Error allocating memory for %s", name);
		vdo_free(info);
		return result;
	}

	result = pthread_create(&thread->thread, NULL, thread_starter, info);
	if (result != 0) {
		result = -errno;
		vdo_log_error_strerror(result, "could not create %s thread",
				       name);
		vdo_free(thread);
		vdo_free(info);
		return result;
	}

	*new_thread = thread;
	return VDO_SUCCESS;
}

/**********************************************************************/
void vdo_join_threads(struct thread *thread)
{
	int result;
	pthread_t pthread;

	result = pthread_join(thread->thread, NULL);
	pthread = thread->thread;
	vdo_free(thread);
	VDO_ASSERT_LOG_ONLY((result == 0), "thread: %p", (void *) pthread);
}

/**********************************************************************/
void initialize_threads_barrier(struct threads_barrier *barrier,
				unsigned int thread_count)
{
	int result;

	result = pthread_barrier_init(&barrier->barrier, NULL, thread_count);
	VDO_ASSERT_LOG_ONLY((result == 0), "pthread_barrier_init error");
}

/**********************************************************************/
void destroy_threads_barrier(struct threads_barrier *barrier)
{
	int result;

	result = pthread_barrier_destroy(&barrier->barrier);
	VDO_ASSERT_LOG_ONLY((result == 0), "pthread_barrier_destroy error");
}

/**********************************************************************/
void enter_threads_barrier(struct threads_barrier *barrier)
{
	int result;

	result = pthread_barrier_wait(&barrier->barrier);
	if (result == PTHREAD_BARRIER_SERIAL_THREAD)
		return;

	VDO_ASSERT_LOG_ONLY((result == 0), "pthread_barrier_wait error");
}
