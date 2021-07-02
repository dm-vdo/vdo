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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/threadsLinuxUser.c#11 $
 */

#include "threads.h"

#include <errno.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "logger.h"
#include "memoryAlloc.h"
#include "permassert.h"
#include "syscalls.h"

/**********************************************************************/
unsigned int get_num_cores(void)
{
	cpu_set_t cpu_set;
	if (sched_getaffinity(0, sizeof(cpu_set), &cpu_set) != 0) {
		uds_log_warning_strerror(errno,
					 "sched_getaffinity() failed, using 1 as number of cores.");
		return 1;
	}

	unsigned int n_cpus = 0;
	for (unsigned int i = 0; i < CPU_SETSIZE; ++i) {
		n_cpus += CPU_ISSET(i, &cpu_set);
	}
	return n_cpus;
}

/**********************************************************************/
void get_thread_name(char *name)
{
	process_control(PR_GET_NAME, (unsigned long) name, 0, 0, 0);
}

/**********************************************************************/
pid_t get_thread_id(void)
{
	return syscall(SYS_gettid);
}

/**********************************************************************/
struct thread_start_info {
	void (*thread_func)(void *);
	void *thread_data;
	const char *name;
};

/**********************************************************************/
static void *thread_starter(void *arg)
{
	struct thread_start_info *tsi = arg;
	void (*thread_func)(void *) = tsi->thread_func;
	void *thread_data = tsi->thread_data;
	/*
	 * The name is just advisory for humans examining it, so we don't
	 * care much if this fails.
	 */
	process_control(PR_SET_NAME, (unsigned long) tsi->name, 0, 0, 0);
	UDS_FREE(tsi);
	thread_func(thread_data);
	return NULL;
}

/**********************************************************************/
int create_thread(void (*thread_func)(void *),
		  void *thread_data,
		  const char *name,
		  struct thread **new_thread)
{
	struct thread_start_info *tsi;
	int result = UDS_ALLOCATE(1, struct thread_start_info, __func__, &tsi);
	if (result != UDS_SUCCESS) {
		return result;
	}
	tsi->thread_func = thread_func;
	tsi->thread_data = thread_data;
	tsi->name = name;

	struct thread *thread;
	result = UDS_ALLOCATE(1, struct thread, __func__, &thread);
	if (result != UDS_SUCCESS) {
		uds_log_warning("Error allocating memory for %s", name);
		UDS_FREE(tsi);
		return result;
	}

	result = pthread_create(&thread->thread, NULL, thread_starter, tsi);
	if (result != 0) {
		uds_log_error_strerror(errno, "could not create %s thread",
				       name);
		UDS_FREE(thread);
		UDS_FREE(tsi);
		return UDS_ENOTHREADS;
	}
	*new_thread = thread;
	return UDS_SUCCESS;
}

/**********************************************************************/
int join_threads(struct thread *th)
{
	int result = pthread_join(th->thread, NULL);
	pthread_t pthread = th->thread;
	UDS_FREE(th);
	return ASSERT_WITH_ERROR_CODE((result == 0), result, "th: %p",
				      (void *)pthread);
}

/**********************************************************************/
int create_thread_key(pthread_key_t *key, void (*destr_function)(void *))
{
	int result = pthread_key_create(key, destr_function);
	return ASSERT_WITH_ERROR_CODE((result == 0), result,
				      "pthread_key_create error");
}

/**********************************************************************/
int delete_thread_key(pthread_key_t key)
{
	int result = pthread_key_delete(key);
	return ASSERT_WITH_ERROR_CODE((result == 0), result,
				      "pthread_key_delete error");
}

/**********************************************************************/
int set_thread_specific(pthread_key_t key, const void *pointer)
{
	int result = pthread_setspecific(key, pointer);
	return ASSERT_WITH_ERROR_CODE((result == 0), result,
				      "pthread_setspecific error");
}

/**********************************************************************/
void *get_thread_specific(pthread_key_t key)
{
	return pthread_getspecific(key);
}

/**********************************************************************/
int initialize_barrier(struct barrier *barrier, unsigned int thread_count)
{
	int result =
		pthread_barrier_init(&barrier->barrier, NULL, thread_count);
	return ASSERT_WITH_ERROR_CODE((result == 0), result,
				      "pthread_barrier_init error");
}

/**********************************************************************/
int destroy_barrier(struct barrier *barrier)
{
	int result = pthread_barrier_destroy(&barrier->barrier);
	return ASSERT_WITH_ERROR_CODE((result == 0), result,
				      "pthread_barrier_destroy error");
}

/**********************************************************************/
int enter_barrier(struct barrier *barrier, bool *winner)
{
	int result = pthread_barrier_wait(&barrier->barrier);

	// Check if this thread is the arbitrary winner and pass that result
	// back as an optional flag instead of overloading the return value.
	if (result == PTHREAD_BARRIER_SERIAL_THREAD) {
		if (winner != NULL) {
			*winner = true;
		}
		return UDS_SUCCESS;
	}

	if (winner != NULL) {
		*winner = false;
	}
	return ASSERT_WITH_ERROR_CODE((result == 0), result,
				      "pthread_barrier_wait error");
}

/**********************************************************************/
int yield_scheduler(void)
{
	int result = sched_yield();
	if (result != 0) {
		return uds_log_error_strerror(errno, "sched_yield failed");
	}

	return UDS_SUCCESS;
}
