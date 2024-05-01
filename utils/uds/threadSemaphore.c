// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include <errno.h>

#include "logger.h"
#include "permassert.h"
#include "thread-utils.h"
#include "time-utils.h"

/**********************************************************************/
int uds_initialize_semaphore(struct semaphore *semaphore, unsigned int value)
{
	int result;

	result = sem_init(&semaphore->semaphore, false, value);
	VDO_ASSERT_LOG_ONLY((result == 0), "sem_init error");
	return result;
}

/**********************************************************************/
int uds_destroy_semaphore(struct semaphore *semaphore)
{
	int result;

	result = sem_destroy(&semaphore->semaphore);
	VDO_ASSERT_LOG_ONLY((result == 0), "sem_destroy error");
	return result;
}

/**********************************************************************/
void uds_acquire_semaphore(struct semaphore *semaphore)
{
	int result;

	do {
		result = sem_wait(&semaphore->semaphore);
	} while ((result == -1) && (errno == EINTR));

#ifndef NDEBUG
	VDO_ASSERT_LOG_ONLY((result == 0), "sem_wait error %d", errno);
#endif
}

/**********************************************************************/
bool uds_attempt_semaphore(struct semaphore *semaphore, ktime_t timeout)
{
	if (timeout > 0) {
		struct timespec ts = future_time(timeout);

		do {
			if (sem_timedwait(&semaphore->semaphore, &ts) == 0)
				return true;
		} while (errno == EINTR);
#ifndef NDEBUG

		VDO_ASSERT_LOG_ONLY((errno == ETIMEDOUT),
				    "sem_timedwait error %d",
				    errno);
#endif
	} else {
		do {
			if (sem_trywait(&semaphore->semaphore) == 0)
				return true;
		} while (errno == EINTR);
#ifndef NDEBUG

		VDO_ASSERT_LOG_ONLY((errno == EAGAIN),
				    "sem_trywait error %d",
				    errno);
#endif
	}

	return false;
}

/**********************************************************************/
void uds_release_semaphore(struct semaphore *semaphore)
{
	int result __attribute__((unused));

	result = sem_post(&semaphore->semaphore);
#ifndef NDEBUG
	VDO_ASSERT_LOG_ONLY((result == 0), "sem_post error %d", errno);
#endif
}
