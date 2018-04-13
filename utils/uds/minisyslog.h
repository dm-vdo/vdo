/*
 * Copyright (c) 2018 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/minisyslog.h#1 $
 */

#ifndef MINISYSLOG_H
#define MINISYSLOG_H 1

#include <syslog.h>
#include <stdarg.h>

/**
 * @file
 *
 * Replacements for the syslog library functions so that the library
 * calls do not conflict with the application calling syslog.
 **/

/**
 * Open the logger. The function mimics the openlog() c-library function.
 *
 * @param ident    The identity string to prepended to all log messages
 * @param option   The logger options (see the openlog(3) man page).
 * @param facility The type of program logging the message.
 **/
void miniOpenlog(const char *ident, int option, int facility);

/**
 * Log a message. This function mimics the syslog() c-library function.
 *
 * @param priority The priority level of the message
 * @param format   A printf style message format
 **/
void miniSyslog(int priority, const char *format, ...)
  __attribute__((format(printf, 2, 3)));

/**
 * Log a message. This function mimics the vsyslog() c-library function.
 *
 * @param priority The priority level of the message
 * @param format   A printf style message format
 * @param ap       An argument list obtained from stdarg()
 **/
void miniVsyslog(int priority, const char *format, va_list ap)
  __attribute__((format(printf, 2, 0)));

/**
 * Log a message pack consisting of multiple variable sections.
 *
 * @param priority      the priority at which to log the message
 * @param prefix        optional string prefix to message, may be NULL
 * @param fmt1          format of message first part, may be NULL
 * @param args1         arguments for message first part
 * @param fmt2          format of message second part, may be NULL
 * @param args2         arguments for message second part
 **/
void miniSyslogPack(int         priority,
                    const char *prefix,
                    const char *fmt1,
                    va_list     args1,
                    const char *fmt2,
                    va_list     args2)
  __attribute__((format(printf, 3, 0), format(printf, 5, 0)));

/**
 * Close a logger. This function mimics the closelog() c-library function.
 **/
void miniCloselog(void);

#endif /* MINI_SYSLOG_H */
