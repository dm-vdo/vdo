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
 * $Id: //eng/uds-releases/gloria/userLinux/uds/memoryDefs.h#1 $
 */

#ifndef LINUX_USER_MEMORY_DEFS_H
#define LINUX_USER_MEMORY_DEFS_H 1

/**
 * Allocate one or more elements of the indicated type, aligning them
 * on the boundary that will allow them to be used in I/O, logging an
 * error if the allocation fails. The memory will be zeroed.
 *
 * @param COUNT  The number of objects to allocate
 * @param TYPE   The type of objects to allocate
 * @param WHAT   What is being allocated (for error logging)
 * @param PTR    A pointer to hold the allocated memory
 *
 * @return UDS_SUCCESS or an error code
 **/
#define ALLOCATE_IO_ALIGNED(COUNT, TYPE, WHAT, PTR) \
  ALLOCATE(COUNT, TYPE, WHAT, PTR)

#endif /* LINUX_USER_MEMORY_DEFS_H */
