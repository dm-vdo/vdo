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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/notificationDefs.h#1 $
 */

#ifndef LINUX_USER_NOTIFICATION_DEFS_H
#define LINUX_USER_NOTIFICATION_DEFS_H

#include "compiler.h"
#include "uds.h"
#include "uds-block.h"

/**
 * Called when a block context is closed.
 *
 * @param context  the block context
 **/
static INLINE void notifyBlockContextClosed(UdsBlockContext context
                                            __attribute__((unused)))
{
}

/**
 * Called when a block context is opened.
 *
 * @param session  the index session
 * @param context  the block context
 **/
static INLINE void notifyBlockContextOpened(UdsIndexSession session
                                            __attribute__((unused)),
                                            UdsBlockContext context
                                            __attribute__((unused)))
{
}

/**
 * Called when an index session is closed.
 *
 * @param session  the index session
 **/
static INLINE void notifyIndexClosed(UdsIndexSession session
                                     __attribute__((unused)))
{
}

/**
 * Called when an index session is opened.
 *
 * @param session  the index session
 * @param name     the index name
 **/
static INLINE void notifyIndexOpened(UdsIndexSession session
                                     __attribute__((unused)),
                                     const char *name __attribute__((unused)))
{
}

#endif /*  LINUX_USER_NOTIFICATION_DEFS_H */
