/*
 * Copyright (c) 2020 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/loggerLinuxUser.c#15 $
 */

#include "logger.h"

#include <execinfo.h>
#include <stdio.h>
#include <unistd.h>

#include "fileUtils.h"
#include "memoryAlloc.h"
#include "stringUtils.h"
#include "threads.h"

const char TIMESTAMPS_ENVIRONMENT_VARIABLE[] = "UDS_LOG_TIMESTAMPS";
const char IDS_ENVIRONMENT_VARIABLE[] = "UDS_LOG_IDS";

static const char IDENTITY[] = "UDS";

static once_state_t logger_once = ONCE_STATE_INITIALIZER;

static unsigned int opened = 0;
static FILE *fp = NULL;
static bool timestamps = true;
static bool ids = true;

/**********************************************************************/
static void init_logger(void)
{
	const char *uds_log_level = getenv("UDS_LOG_LEVEL");
	if (uds_log_level != NULL) {
		set_log_level(string_to_priority(uds_log_level));
	} else {
		set_log_level(LOG_INFO);
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
				FREE(log_file);
			}
			opened = 1;
			return;
		}
		error = errno;
	}

	char *identity;
	if (alloc_sprintf(NULL, &identity, "%s/%s", IDENTITY,
			  program_invocation_short_name) == UDS_SUCCESS) {
		mini_openlog(identity, LOG_PID | LOG_NDELAY | LOG_CONS,
			     LOG_USER);
		FREE(identity);
	} else {
		mini_openlog(IDENTITY, LOG_PID | LOG_NDELAY | LOG_CONS,
			     LOG_USER);
		logError("Could not include program name in log");
	}

	if (error != 0) {
		logErrorWithStringError(error, "Couldn't open log file %s",
					log_file);
	}

	if (is_abs_path) {
		FREE(log_file);
	}
	opened = 1;
}

/**********************************************************************/
void open_logger(void)
{
	perform_once(&logger_once, init_logger);
}

/**********************************************************************/
static void format_current_time(char *buffer, size_t buffer_size)
{
	*buffer = 0;

	ktime_t now = currentTime(CLOCK_REALTIME);
	struct timeval tv = asTimeVal(now);

	struct tm tmp;
	if (localtime_r(&tv.tv_sec, &tmp) == NULL) {
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
		 (int) (tv.tv_usec / 1000));
}

/**********************************************************************/
void log_message_pack(int priority,
		      const char *prefix,
		      const char *fmt1,
		      va_list args1,
		      const char *fmt2,
		      va_list args2)
{
	open_logger();
	if (priority > get_log_level()) {
		return;
	}

	// Preserve errno since the caller cares more about their own error
	// state than about errors in the logging code.
	int error = errno;

	if (fp == NULL) {
		mini_syslog_pack(priority, prefix, fmt1, args1, fmt2, args2);
	} else {
		char tname[16];
		get_thread_name(tname);
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

		fprintf(fp, ": %-6s (%s", priority_to_string(priority), tname);

		if (ids) {
			fprintf(fp, "/%d", get_thread_id());
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
__attribute__((format(printf, 2, 3))) static void
log_at_level(int priority, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	v_log_message(priority, format, args);
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

	log_at_level(priority, "maps file");
	char buffer[1024];
	char *map_line;
	while ((map_line = fgets(buffer, 1024, maps_file)) != NULL) {
		char *newline = strchr(map_line, '\n');
		if (newline != NULL) {
			*newline = '\0';
		}
		log_at_level(priority, "  %s", map_line);
	}
	log_at_level(priority, "end of maps file");

	fclose(maps_file);
}

enum { NUM_STACK_FRAMES = 32 };

/**********************************************************************/
void log_backtrace(int priority)
{
	log_at_level(priority, "[Call Trace:]");
	void *trace[NUM_STACK_FRAMES];
	int trace_size = backtrace(trace, NUM_STACK_FRAMES);
	char **messages = backtrace_symbols(trace, trace_size);
	if (messages == NULL) {
		log_at_level(priority, "backtrace failed");
	} else {
		for (int i = 0; i < trace_size; ++i) {
			log_at_level(priority, "  %s", messages[i]);
		}
		// "messages" is malloc'ed indirectly by backtrace_symbols
		free(messages);
		log_proc_maps(priority);
	}
}

/**********************************************************************/
void pause_for_logger(void)
{
	// User-space logger can't be overrun, so this is a no-op.
}

