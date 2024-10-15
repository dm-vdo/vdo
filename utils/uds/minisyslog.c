// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "logger.h"
#include "memory-alloc.h"
#include "minisyslog.h"
#include "string-utils.h"
#include "thread-utils.h"
#include "time-utils.h"

static struct mutex mutex = UDS_MUTEX_INITIALIZER;

static int log_socket = -1;

static char *log_ident;

static int log_option;

static int default_facility = LOG_USER;

/**********************************************************************/
static void close_locked(void)
{
	if (log_socket != -1) {
		close(log_socket);
		log_socket = -1;
	}
}

/**********************************************************************/
static void open_socket_locked(void)
{
	if (log_socket != -1)
		return;

	struct sockaddr_un sun;
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, _PATH_LOG, sizeof(sun.sun_path));

	/*
	 * We can't log from here, we'll deadlock, so we can't use
	 * loggingSocket(), loggingConnect(), or tryCloseFile().
	 */
	log_socket = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (log_socket < 0)
		return;

	if (connect(log_socket, (const struct sockaddr *) &sun, sizeof(sun)) !=
	    0)
		close_locked();
}

/**********************************************************************/
void mini_openlog(const char *ident, int option, int facility)
{
	uds_lock_mutex(&mutex);
	close_locked();
	vdo_free(log_ident);
	if (vdo_duplicate_string(ident, NULL, &log_ident) != VDO_SUCCESS)
		// on failure, NULL is okay
		log_ident = NULL;
	log_option = option;
	default_facility = facility;
	if (log_option & LOG_NDELAY)
		open_socket_locked();
	uds_unlock_mutex(&mutex);
}

/**********************************************************************/
void mini_syslog(int priority, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	mini_vsyslog(priority, format, args);
	va_end(args);
}

/**********************************************************************/
static bool write_msg(int fd, const char *msg)
{
	size_t bytes_to_write = strlen(msg);
	ssize_t bytes_written = write(fd, msg, bytes_to_write);
	if (bytes_written == (ssize_t) bytes_to_write) {
		bytes_to_write += 1;
		bytes_written += write(fd, "\n", 1);
	}
	return bytes_written != (ssize_t) bytes_to_write;
}

/**********************************************************************/
__printf(3, 0)
#ifdef __clang__
// Clang insists on annotating both printf style format strings, but
// gcc doesn't understand the second.
__printf(5, 0)
#endif //__clang__
static void log_it(int priority,
		   const char *prefix,
		   const char *format1,
		   va_list args1,
		   const char *format2,
		   va_list args2)
{
	const char *priority_str = vdo_log_priority_to_string(priority);
	char buffer[1024];
	char *buf_end = buffer + sizeof(buffer);
	char *bufp = buffer;
	time_t t = ktime_to_seconds(current_time_ns(CLOCK_REALTIME));
	struct tm tm;
	char timestamp[64];
	timestamp[0] = '\0';
	if (localtime_r(&t, &tm) != NULL)
		if (strftime(timestamp,
			     sizeof(timestamp),
			     "%b %e %H:%M:%S",
			     &tm) == 0)
			timestamp[0] = '\0';
	if (LOG_FAC(priority) == 0)
		priority |= default_facility;

	bufp = vdo_append_to_buffer(bufp, buf_end, "<%d>%s", priority,
				    timestamp);
	const char *stderr_msg = bufp;
	bufp = vdo_append_to_buffer(bufp, buf_end, " %s",
				    log_ident == NULL ? "" : log_ident);

	if (log_option & LOG_PID) {
		char tname[16];
		uds_get_thread_name(tname);
		bufp = vdo_append_to_buffer(bufp,
					    buf_end,
					    "[%u]: %-6s (%s/%d) ",
					    getpid(),
					    priority_str,
					    tname,
					    uds_get_thread_id());
	} else {
		bufp = vdo_append_to_buffer(bufp, buf_end, ": ");
	}
	if ((bufp + sizeof("...")) >= buf_end)
		return;
	if (prefix != NULL)
		bufp = vdo_append_to_buffer(bufp, buf_end, "%s", prefix);
	if (format1 != NULL) {
		int ret = vsnprintf(bufp, buf_end - bufp, format1, args1);
		if (ret < (buf_end - bufp))
			bufp += ret;
		else
			bufp = buf_end;
		}
	if (format2 != NULL) {
		int ret = vsnprintf(bufp, buf_end - bufp, format2, args2);
		if (ret < (buf_end - bufp))
			bufp += ret;
		else
			bufp = buf_end;
		}
	if (bufp == buf_end)
		strcpy(buf_end - sizeof("..."), "...");
	bool failure = false;
	if (log_option & LOG_PERROR)
		failure |= write_msg(STDERR_FILENO, stderr_msg);
	open_socket_locked();
	failure |= (log_socket == -1);
	if (log_socket != -1) {
		size_t bytes_to_write = bufp - buffer;
		ssize_t bytes_written =
			send(log_socket, buffer, bytes_to_write, MSG_NOSIGNAL);
		failure |= (bytes_written != (ssize_t) bytes_to_write);
	}
	if (failure && (log_option & LOG_CONS)) {
		int console = open(_PATH_CONSOLE, O_WRONLY);
		if (console != -1) {
			write_msg(console, stderr_msg);
			close(console);
		}
	}
}

void mini_syslog_pack(int priority,
		      const char *prefix,
		      const char *fmt1,
		      va_list args1,
		      const char *fmt2,
		      va_list args2)
{
	uds_lock_mutex(&mutex);
	log_it(priority, prefix, fmt1, args1, fmt2, args2);
	uds_unlock_mutex(&mutex);
}

void mini_vsyslog(int priority, const char *format, va_list ap)
{
	va_list dummy;
	memset(&dummy, 0, sizeof(dummy));
	uds_lock_mutex(&mutex);
	log_it(priority, NULL, format, ap, NULL, dummy);
	uds_unlock_mutex(&mutex);
}

void mini_closelog(void)
{
	uds_lock_mutex(&mutex);
	close_locked();
	vdo_free(log_ident);
	log_ident = NULL;
	log_option = 0;
	default_facility = LOG_USER;
	uds_unlock_mutex(&mutex);
}
