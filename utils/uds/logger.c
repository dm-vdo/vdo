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

#include "logger.h"

#include <execinfo.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

#include "fileUtils.h"
#include "memoryAlloc.h"
#include "stringUtils.h"
#include "uds-threads.h"

typedef struct {
	const char *name;
	const int priority;
} PriorityName;

static const PriorityName PRIORITIES[] = {
	{ "ALERT", UDS_LOG_ALERT },
	{ "CRITICAL", UDS_LOG_CRIT },
	{ "CRIT", UDS_LOG_CRIT },
	{ "DEBUG", UDS_LOG_DEBUG },
	{ "EMERGENCY", UDS_LOG_EMERG },
	{ "EMERG", UDS_LOG_EMERG },
	{ "ERROR", UDS_LOG_ERR },
	{ "ERR", UDS_LOG_ERR },
	{ "INFO", UDS_LOG_INFO },
	{ "NOTICE", UDS_LOG_NOTICE },
	{ "PANIC", UDS_LOG_EMERG },
	{ "WARN", UDS_LOG_WARNING },
	{ "WARNING", UDS_LOG_WARNING },
	{ NULL, -1 },
};

static const char *const PRIORITY_STRINGS[] = {
	"EMERGENCY",
	"ALERT",
	"CRITICAL",
	"ERROR",
	"WARN",
	"NOTICE",
	"INFO",
	"DEBUG",
};

static int log_level = UDS_LOG_INFO;

const char TIMESTAMPS_ENVIRONMENT_VARIABLE[] = "UDS_LOG_TIMESTAMPS";
const char IDS_ENVIRONMENT_VARIABLE[] = "UDS_LOG_IDS";

static const char IDENTITY[] = "UDS";

static once_state_t logger_once = ONCE_STATE_INITIALIZER;

static unsigned int opened = 0;
static FILE *fp = NULL;
static bool timestamps = true;
static bool ids = true;


/**********************************************************************/
int get_uds_log_level(void)
{
	return log_level;
}

/**********************************************************************/
void set_uds_log_level(int new_log_level)
{
	log_level = new_log_level;
}

/**********************************************************************/
int uds_log_string_to_priority(const char *string)
{
	int i;
	for (i = 0; PRIORITIES[i].name != NULL; i++) {
		if (strcasecmp(string, PRIORITIES[i].name) == 0) {
			return PRIORITIES[i].priority;
		}
	}
	return UDS_LOG_INFO;
}

/**********************************************************************/
const char *uds_log_priority_to_string(int priority)
{
	if ((priority < 0) ||
	    (priority >= (int) ARRAY_SIZE(PRIORITY_STRINGS))) {
		return "unknown";
	}
	return PRIORITY_STRINGS[priority];
}

/**********************************************************************/
static void init_logger(void)
{
	const char *uds_log_level = getenv("UDS_LOG_LEVEL");
	if (uds_log_level != NULL) {
		set_uds_log_level(uds_log_string_to_priority(uds_log_level));
	} else {
		set_uds_log_level(UDS_LOG_INFO);
	}

	char *timestamps_string = getenv(TIMESTAMPS_ENVIRONMENT_VARIABLE);
	if (timestamps_string != NULL && strcmp(timestamps_string, "0") == 0) {
		timestamps = false;
	}

	char *ids_string = getenv(IDS_ENVIRONMENT_VARIABLE);
	if (ids_string != NULL && strcmp(ids_string, "0") == 0) {
		ids = false;
	}

	int error = 0;
	char *log_file = getenv("UDS_LOGFILE");
	bool is_abs_path = false;
	if (log_file != NULL) {
		is_abs_path =
			(make_abs_path(log_file, &log_file) == UDS_SUCCESS);
		errno = 0;
		fp = fopen(log_file, "a");
		if (fp != NULL) {
			if (is_abs_path) {
				UDS_FREE(log_file);
			}
			opened = 1;
			return;
		}
		error = errno;
	}

	char *identity;
	if (uds_alloc_sprintf(NULL, &identity, "%s/%s", IDENTITY,
			      program_invocation_short_name) == UDS_SUCCESS) {
		mini_openlog(identity, LOG_PID | LOG_NDELAY | LOG_CONS,
			     LOG_USER);
		UDS_FREE(identity);
	} else {
		mini_openlog(IDENTITY, LOG_PID | LOG_NDELAY | LOG_CONS,
			     LOG_USER);
		uds_log_error("Could not include program name in log");
	}

	if (error != 0) {
		uds_log_error_strerror(error, "Couldn't open log file %s",
				       log_file);
	}

	if (is_abs_path) {
		UDS_FREE(log_file);
	}
	opened = 1;
}

/**********************************************************************/
void open_uds_logger(void)
{
	perform_once(&logger_once, init_logger);
}

/**********************************************************************/
static void format_current_time(char *buffer, size_t buffer_size)
{
	*buffer = 0;

	ktime_t now = current_time_ns(CLOCK_REALTIME);
	struct tm tmp;
	const time_t seconds = now / NSEC_PER_SEC;
	if (localtime_r(&seconds, &tmp) == NULL) {
		return;
	}

	if (strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &tmp) == 0) {
		*buffer = 0;
		return;
	}

	size_t current_length = strlen(buffer);
	if (current_length > (buffer_size - 5)) {
		// Not enough room to add milliseconds but we do have a time
		// string.
		return;
	}
	snprintf(buffer + current_length,
		 buffer_size - current_length,
		 ".%03d",
		 (int) ((now % NSEC_PER_SEC) / NSEC_PER_MSEC));
}

/**********************************************************************/
void uds_log_message_pack(int priority,
			  const char *module __always_unused,
			  const char *prefix,
			  const char *fmt1,
			  va_list args1,
			  const char *fmt2,
			  va_list args2)
	{
	open_uds_logger();
	if (priority > get_uds_log_level()) {
		return;
	}

	// Preserve errno since the caller cares more about their own error
	// state than about errors in the logging code.
	int error = errno;

	if (fp == NULL) {
		mini_syslog_pack(priority, prefix, fmt1, args1, fmt2, args2);
	} else {
		char tname[16];
		uds_get_thread_name(tname);
		flockfile(fp);

		if (timestamps) {
			char time_buffer[32];
			format_current_time(time_buffer, sizeof(time_buffer));
			fprintf(fp, "%s ", time_buffer);
		}

		fputs(program_invocation_short_name, fp);

		if (ids) {
			fprintf(fp, "[%u]", getpid());
		}

		fprintf(fp, ": %-6s (%s", uds_log_priority_to_string(priority),
			tname);

		if (ids) {
			fprintf(fp, "/%d", uds_get_thread_id());
		}

		fputs(") ", fp);

		if (prefix != NULL) {
			fputs(prefix, fp);
		}
		if (fmt1 != NULL) {
			vfprintf(fp, fmt1, args1);
		}
		if (fmt2 != NULL) {
			vfprintf(fp, fmt2, args2);
		}
		fputs("\n", fp);
		fflush(fp);
		funlockfile(fp);
	}

	// Reset errno
	errno = error;
}

/**********************************************************************/
void uds_log_embedded_message(int priority,
			      const char *module,
			      const char *prefix,
			      const char *fmt1,
			      va_list args1,
			      const char *fmt2,
			      ...)
{
	va_list ap;
	va_start(ap, fmt2);
	uds_log_message_pack(priority, module, prefix, fmt1, args1, fmt2, ap);
	va_end(ap);
}

/**********************************************************************/
int uds_vlog_strerror(int priority,
		      int errnum,
		      const char *module,
		      const char *format,
		      va_list args)
{
	char errbuf[ERRBUF_SIZE];
        const char *message = uds_string_error(errnum, errbuf, sizeof(errbuf));
	uds_log_embedded_message(priority,
				 module,
				 NULL,
				 format,
				 args,
				 ": %s (%u)",
				 message,
				 errnum);
	return errnum;
}

/**********************************************************************/
int __uds_log_strerror(int priority,
		       int errnum,
		       const char *module,
		       const char *format,
		       ...)
{
	va_list args;

	va_start(args, format);
	uds_vlog_strerror(priority, errnum, module, format, args);
	va_end(args);
	return errnum;
}

/**********************************************************************/
void uds_log_message(int priority, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	uds_log_embedded_message(priority, NULL, NULL, format, args, "%s", "");
	va_end(args);
}

/**
 * Log the contents of /proc/self/maps so that we can decode the addresses
 * in a stack trace.
 *
 * @param priority The priority at which to log
 **/
static void log_proc_maps(int priority)
{
	FILE *maps_file = fopen("/proc/self/maps", "r");
	if (maps_file == NULL) {
		return;
	}

	uds_log_message(priority, "maps file");
	char buffer[1024];
	char *map_line;
	while ((map_line = fgets(buffer, 1024, maps_file)) != NULL) {
		char *newline = strchr(map_line, '\n');
		if (newline != NULL) {
			*newline = '\0';
		}
		uds_log_message(priority, "  %s", map_line);
	}
	uds_log_message(priority, "end of maps file");

	fclose(maps_file);
}

enum { NUM_STACK_FRAMES = 32 };

/**********************************************************************/
void uds_log_backtrace(int priority)
{
	uds_log_message(priority, "[Call Trace:]");
	void *trace[NUM_STACK_FRAMES];
	int trace_size = backtrace(trace, NUM_STACK_FRAMES);
	char **messages = backtrace_symbols(trace, trace_size);
	if (messages == NULL) {
		uds_log_message(priority, "backtrace failed");
	} else {
		for (int i = 0; i < trace_size; ++i) {
			uds_log_message(priority, "  %s", messages[i]);
		}
		// "messages" is malloc'ed indirectly by backtrace_symbols
		free(messages);
		log_proc_maps(priority);
	}
}

/**********************************************************************/
void uds_pause_for_logger(void)
{
	// User-space logger can't be overrun, so this is a no-op.
}

