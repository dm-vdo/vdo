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
 * $Id: //eng/uds-releases/flanders/userLinux/uds/isCallbackThreadDefs.h#2 $
 */

#ifndef LINUX_USER_IS_CALLBACK_THREAD_DEFS_H
#define LINUX_USER_IS_CALLBACK_THREAD_DEFS_H 1

#include "compiler.h"
#include "threadDefs.h"
#include "typeDefs.h"

// Set this macro because we can accurately identify the callback thread
#define CAN_IDENTIFY_CALLBACK_THREAD 1

// Key for the thread-specific flag indicating an active callback
pthread_key_t threadLimitKey;

/**********************************************************************/
static INLINE void createCallbackThread(void)
{
  createThreadKey(&threadLimitKey, NULL);
}

/**********************************************************************/
static INLINE void deleteCallbackThread(void)
{
  deleteThreadKey(threadLimitKey);
}

/**********************************************************************/
static INLINE bool isCallbackThread(void)
{
  return (getThreadSpecific(threadLimitKey) != NULL);
}

/**********************************************************************/
static INLINE void setCallbackThread(const void *p)
{
    setThreadSpecific(threadLimitKey, p);
}

#endif /* LINUX_USER_IS_CALLBACK_THREAD_DEFS_H */
