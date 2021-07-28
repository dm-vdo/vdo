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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/threadSemaphoreLinuxUser.c#8 $
 */

#include <errno.h>

#include "logger.h"
#include "permassert.h"
#include "threads.h"
#include "timeUtils.h"

/**********************************************************************/
int uds_initialize_semaphore(struct semaphore *semaphore, unsigned int value)
{
	int result = sem_init(&semaphore->semaphore, false, value);
	return ASSERT_WITH_ERROR_CODE((result == 0), result, "sem_init error");
}

/**********************************************************************/
int uds_destroy_semaphore(struct semaphore *semaphore)
{
	int result = sem_destroy(&semaphore->semaphore);
	return ASSERT_WITH_ERROR_CODE((result == 0), result,
				      "sem_destroy error");
}

/**********************************************************************/
void uds_acquire_semaphore(struct semaphore *semaphore)
{
	int result;
	do {
		result = sem_wait(&semaphore->semaphore);
	} while ((result == -1) && (errno == EINTR));

#ifndef NDEBUG
	ASSERT_LOG_ONLY((result == 0), "sem_wait error %d", errno);
#endif
}

/**********************************************************************/
bool uds_attempt_semaphore(struct semaphore *semaphore, ktime_t timeout)
{
	if (timeout > 0) {
		struct timespec ts = future_time(timeout);
		do {
			if (sem_timedwait(&semaphore->semaphore, &ts) == 0) {
				return true;
			}
		} while (errno == EINTR);
#ifndef NDEBUG
		ASSERT_LOG_ONLY((errno == ETIMEDOUT), "sem_timedwait error %d",
				errno);
#endif
	} else {
		do {
			if (sem_trywait(&semaphore->semaphore) == 0) {
				return true;
			}
		} while (errno == EINTR);
#ifndef NDEBUG
		ASSERT_LOG_ONLY((errno == EAGAIN), "sem_trywait error %d",
				errno);
#endif
	}
	return false;
}

/**********************************************************************/
void uds_release_semaphore(struct semaphore *semaphore)
{
	int result __attribute__((unused)) = sem_post(&semaphore->semaphore);
#ifndef NDEBUG
	ASSERT_LOG_ONLY((result == 0), "sem_post error %d", errno);
#endif
}
