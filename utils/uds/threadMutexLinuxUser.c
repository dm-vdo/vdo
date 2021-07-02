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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/threadMutexLinuxUser.c#7 $
 */

#include <errno.h>
#include <stdlib.h>

#include "permassert.h"
#include "stringUtils.h"
#include "threadOnce.h"
#include "threads.h"

static enum mutex_kind {
	fast_adaptive,
	error_checking
} hidden_mutex_kind = error_checking;

const bool UDS_DO_ASSERTIONS = true;

/**********************************************************************/
static void initialize_mutex_kind(void)
{
	static const char UDS_MUTEX_KIND_ENV[] = "UDS_MUTEX_KIND";

	// Enabling error checking on mutexes enables a great performance loss,
	// so we only enable it in certain circumstances.
#ifdef NDEBUG
	hidden_mutex_kind = fast_adaptive;
#endif

	const char *mutex_kind_string = getenv(UDS_MUTEX_KIND_ENV);
	if (mutex_kind_string != NULL) {
		if (strcmp(mutex_kind_string, "error-checking") == 0) {
			hidden_mutex_kind = error_checking;
		} else if (strcmp(mutex_kind_string, "fast-adaptive") == 0) {
			hidden_mutex_kind = fast_adaptive;
		} else {
			ASSERT_LOG_ONLY(false,
					"environment variable %s had unexpected value '%s'",
					UDS_MUTEX_KIND_ENV,
					mutex_kind_string);
		}
	}
}

/**********************************************************************/
static enum mutex_kind get_mutex_kind(void)
{
	static once_state_t once_state = ONCE_STATE_INITIALIZER;

	perform_once(&once_state, initialize_mutex_kind);

	return hidden_mutex_kind;
}

/**********************************************************************/
int uds_initialize_mutex(struct mutex *mutex, bool assert_on_error)
{
	pthread_mutexattr_t attr;
	int result = pthread_mutexattr_init(&attr);
	if (result != 0) {
		return ASSERT_WITH_ERROR_CODE((result == 0), result,
					      "pthread_mutexattr_init error");
	}
	if (get_mutex_kind() == error_checking) {
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	}
	result = pthread_mutex_init(&mutex->mutex, &attr);
	if ((result != 0) && assert_on_error) {
		result = ASSERT_WITH_ERROR_CODE((result == 0), result,
						"pthread_mutex_init error");
	}
	int result2 = pthread_mutexattr_destroy(&attr);
	if (result2 != 0) {
		ASSERT_LOG_ONLY((result2 == 0),
				"pthread_mutexattr_destroy error");
		if (result == UDS_SUCCESS) {
			result = result2;
		}
	}
	return result;
}

/**********************************************************************/
int uds_init_mutex(struct mutex *mutex)
{
	return uds_initialize_mutex(mutex, UDS_DO_ASSERTIONS);
}

/**********************************************************************/
int uds_destroy_mutex(struct mutex *mutex)
{
	int result = pthread_mutex_destroy(&mutex->mutex);
	return ASSERT_WITH_ERROR_CODE((result == 0), result,
				      "pthread_mutex_destroy error");
}

/**********************************************************************
 * Error checking for mutex calls is only enabled when NDEBUG is not
 * defined. So only check for and report errors when the mutex calls can
 * return errors.
 */

/**********************************************************************/
void uds_lock_mutex(struct mutex *mutex)
{
	int result __attribute__((unused)) = pthread_mutex_lock(&mutex->mutex);
#ifndef NDEBUG
	ASSERT_LOG_ONLY((result == 0), "pthread_mutex_lock error %d", result);
#endif
}

/**********************************************************************/
void uds_unlock_mutex(struct mutex *mutex)
{
	int result __attribute__((unused)) =
		pthread_mutex_unlock(&mutex->mutex);
#ifndef NDEBUG
	ASSERT_LOG_ONLY((result == 0),
			"pthread_mutex_unlock error %d", result);
#endif
}
