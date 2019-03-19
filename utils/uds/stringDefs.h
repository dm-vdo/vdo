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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/stringDefs.h#1 $
 */

#ifndef LINUX_USER_STRING_DEFS_H
#define LINUX_USER_STRING_DEFS_H

#include <stdio.h>   // for vsnprintf
#include <stdlib.h>  // for strtol
#include <string.h>
#include <strings.h>

/**
 * Scan a string in a sscanf() style.
 *
 * @param [in]  what     A description of what is being scanned, for error
 *                       logging; if NULL doesn't log anything.
 * @param [in]  numItems The number of items we expect to convert, >= 1.
 * @param [in]  str      The string to scan.
 * @param [in]  fmt      The sscanf format parameter.
 * @return <code>UDS_SUCCESS</code> or <code>error</code>
 **/
int scanString(const char *what, int numItems, const char *str,
               const char *fmt, ...)
  __attribute__((format(scanf, 4, 5), warn_unused_result));

#endif /* LINUX_USER_STRING_DEFS_H */
