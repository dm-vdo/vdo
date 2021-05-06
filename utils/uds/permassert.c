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
 * $Id: //eng/uds-releases/krusty/src/uds/permassert.c#9 $
 */

#include "permassert.h"

#include "errors.h"
#include "logger.h"

// Here begins a large block of userspace-only stuff.
#ifdef NDEBUG
#define DEBUGGING_OFF
#undef NDEBUG
#endif /* NDEBUG */

#include <assert.h>
#include <stdlib.h>
#include <syslog.h>

#include "common.h"
#include "stringUtils.h"
#include "threads.h"

#ifdef DEBUGGING_OFF
static bool exit_on_assertion_failure = false;
#else /* !DEBUGGING_OFF */
static bool exit_on_assertion_failure = true;
#endif /* DEBUGGING_OFF */

static const char *EXIT_ON_ASSERTION_FAILURE_VARIABLE =
	"UDS_EXIT_ON_ASSERTION_FAILURE";

static once_state_t init_once = ONCE_STATE_INITIALIZER;
static struct mutex mutex = { .mutex = MUTEX_INITIALIZER };

/**********************************************************************/
static void initialize(void)
{
	initialize_mutex(&mutex, !DO_ASSERTIONS);
	char *exit_on_assertion_failure_string =
		getenv(EXIT_ON_ASSERTION_FAILURE_VARIABLE);
	if (exit_on_assertion_failure_string != NULL) {
		exit_on_assertion_failure =
			(strcasecmp(exit_on_assertion_failure_string,
				    "true") == 0);
	}
}

/**********************************************************************/
bool set_exit_on_assertion_failure(bool should_exit)
{
	perform_once(&init_once, initialize);
	lock_mutex(&mutex);
	bool previous_setting = exit_on_assertion_failure;
	exit_on_assertion_failure = should_exit;
	unlock_mutex(&mutex);
	return previous_setting;
}

// Here ends large block of userspace stuff.

/**********************************************************************/
int uds_assertion_failed(const char *expression_string,
			 int code,
			 const char *file_name,
			 int line_number,
			 const char *format,
			 ...)
{
	// XXX plumb module_name through to here
	const char *module_name = NULL;
	va_list args;
	va_start(args, format);

	uds_log_embedded_message(LOG_ERR,
				 module_name,
				 "assertion \"",
				 format,
				 args,
				 "\" (%s) failed at %s:%d",
				 expression_string,
				 file_name,
				 line_number);
	uds_log_backtrace(LOG_ERR);

	perform_once(&init_once, initialize);
	lock_mutex(&mutex);
	if (exit_on_assertion_failure) {
		__assert_fail(expression_string,
			      file_name,
			      line_number,
			      __ASSERT_FUNCTION);
	}
	unlock_mutex(&mutex);

	va_end(args);

	return code;
}
