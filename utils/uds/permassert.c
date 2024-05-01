// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include "permassert.h"

#include "errors.h"
#include "logger.h"

#ifdef NDEBUG
#define DEBUGGING_OFF
#undef NDEBUG
#endif /* NDEBUG */

#include <assert.h>
#include <stdlib.h>
#include <strings.h>
#include <syslog.h>

#include "string-utils.h"
#include "thread-utils.h"

#ifdef DEBUGGING_OFF
static bool exit_on_assertion_failure;
#else /* not DEBUGGING_OFF */
static bool exit_on_assertion_failure = true;
#endif /* DEBUGGING_OFF */

static const char *EXIT_ON_ASSERTION_FAILURE_VARIABLE = "UDS_EXIT_ON_ASSERTION_FAILURE";

static atomic_t init_once = ATOMIC_INIT(0);
static struct mutex mutex = UDS_MUTEX_INITIALIZER;

static void initialize(void)
{
	uds_initialize_mutex(&mutex, !UDS_DO_ASSERTIONS);
	char *exit_on_assertion_failure_string = getenv(EXIT_ON_ASSERTION_FAILURE_VARIABLE);
	if (exit_on_assertion_failure_string != NULL) {
		exit_on_assertion_failure =
			(strcasecmp(exit_on_assertion_failure_string, "true") == 0);
	}
}

bool set_exit_on_assertion_failure(bool should_exit)
{
	bool previous_setting;

	vdo_perform_once(&init_once, initialize);
	uds_lock_mutex(&mutex);
	previous_setting = exit_on_assertion_failure;
	exit_on_assertion_failure = should_exit;
	uds_unlock_mutex(&mutex);
	return previous_setting;
}

int vdo_assertion_failed(const char *expression_string, const char *file_name,
			 int line_number, const char *format, ...)
{
	va_list args;

	va_start(args, format);

	vdo_log_embedded_message(VDO_LOG_ERR, VDO_LOGGING_MODULE_NAME, "assertion \"",
				 format, args, "\" (%s) failed at %s:%d",
				 expression_string, file_name, line_number);
	vdo_log_backtrace(VDO_LOG_ERR);

	vdo_perform_once(&init_once, initialize);
	uds_lock_mutex(&mutex);
	if (exit_on_assertion_failure) {
		__assert_fail(expression_string, file_name, line_number,
			      __ASSERT_FUNCTION);
	}
	uds_unlock_mutex(&mutex);

	va_end(args);

	return UDS_ASSERTION_FAILED;
}
