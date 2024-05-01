/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2023 Red Hat
 */

#ifndef VDO_LOGGER_H
#define VDO_LOGGER_H

#include <stdarg.h>
#include "minisyslog.h"

/* Custom logging utilities for UDS */

#define VDO_LOG_EMERG LOG_EMERG
#define VDO_LOG_ALERT LOG_ALERT
#define VDO_LOG_CRIT LOG_CRIT
#define VDO_LOG_ERR LOG_ERR
#define VDO_LOG_WARNING LOG_WARNING
#define VDO_LOG_NOTICE LOG_NOTICE
#define VDO_LOG_INFO LOG_INFO
#define VDO_LOG_DEBUG LOG_DEBUG

#define VDO_LOGGING_MODULE_NAME "vdo"

/* Apply a rate limiter to a log method call. */
#define vdo_log_ratelimit(log_fn, ...) log_fn(__VA_ARGS__)

int vdo_get_log_level(void);

int vdo_log_string_to_priority(const char *string);

const char *vdo_log_priority_to_string(int priority);

void vdo_log_embedded_message(int priority, const char *module, const char *prefix,
			      const char *fmt1, va_list args1, const char *fmt2, ...)
	__printf(4, 0) __printf(6, 7);

void vdo_log_backtrace(int priority);

/* All log functions will preserve the caller's value of errno. */

#define vdo_log_strerror(priority, errnum, ...) \
	__vdo_log_strerror(priority, errnum, VDO_LOGGING_MODULE_NAME, __VA_ARGS__)

int __vdo_log_strerror(int priority, int errnum, const char *module,
		       const char *format, ...)
	__printf(4, 5);

int vdo_vlog_strerror(int priority, int errnum, const char *module, const char *format,
		      va_list args)
	__printf(4, 0);

/* Log an error prefixed with the string associated with the errnum. */
#define vdo_log_error_strerror(errnum, ...) \
	vdo_log_strerror(VDO_LOG_ERR, errnum, __VA_ARGS__)

#define vdo_log_debug_strerror(errnum, ...) \
	vdo_log_strerror(VDO_LOG_DEBUG, errnum, __VA_ARGS__)

#define vdo_log_info_strerror(errnum, ...) \
	vdo_log_strerror(VDO_LOG_INFO, errnum, __VA_ARGS__)

#define vdo_log_warning_strerror(errnum, ...) \
	vdo_log_strerror(VDO_LOG_WARNING, errnum, __VA_ARGS__)

#define vdo_log_fatal_strerror(errnum, ...) \
	vdo_log_strerror(VDO_LOG_CRIT, errnum, __VA_ARGS__)

void vdo_log_message(int priority, const char *format, ...)
	__printf(2, 3);

#define vdo_log_debug(...) vdo_log_message(VDO_LOG_DEBUG, __VA_ARGS__)

#define vdo_log_info(...) vdo_log_message(VDO_LOG_INFO, __VA_ARGS__)

#define vdo_log_warning(...) vdo_log_message(VDO_LOG_WARNING, __VA_ARGS__)

#define vdo_log_error(...) vdo_log_message(VDO_LOG_ERR, __VA_ARGS__)

#define vdo_log_fatal(...) vdo_log_message(VDO_LOG_CRIT, __VA_ARGS__)

void vdo_pause_for_logger(void);

void open_vdo_logger(void);
#endif /* VDO_LOGGER_H */
