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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/compilerDefs.h#1 $
 */

#ifndef LINUX_USER_COMPILER_DEFS_H
#define LINUX_USER_COMPILER_DEFS_H

#define container_of(ptr, type, member)               \
  __extension__ ({                                    \
    __typeof__(((type *)0)->member) *__mptr = (ptr);  \
    (type *)((char *)__mptr - offsetof(type,member)); \
  })

/**
 * CPU Branch-prediction hints, courtesy of GCC. Defining these as inline
 * functions instead of macros spoils their magic, sadly.
 **/
#define likely(expr)    __builtin_expect(!!(expr), 1)
#define unlikely(expr)  __builtin_expect(!!(expr), 0)

#endif /* LINUX_USER_COMPILER_DEFS_H */
