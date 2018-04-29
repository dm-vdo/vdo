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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/fileIndexState.h#1 $
 */

#ifndef FILE_INDEX_STATE_H
#define FILE_INDEX_STATE_H 1

#include "indexStateInternals.h"
#include "permassert.h"

typedef struct fileIndexState {
  IndexState  state;            ///< main index state information
  char       *current;          ///< path to current sub-directory
  char       *next;             ///< path to next sub-directory
  char       *previous;         ///< path to previous sub-directory
  char       *deletion;         ///< path to temporary deletion directory
} FileIndexState;

/**
 * Allocate a file index state structure.
 *
 * @param directory     The pathname to the directory containg the index
 *                      state sub-dirs.
 * @param id            The sub-index id for this index.
 * @param zoneCount     The number of index zones in use.
 * @param length        Number of components to hold.
 * @param statePtr      The pointer to hold the new index state.
 *
 * @return              UDS_SUCCESS or a failure code.
 **/
int makeFileIndexState(const char    *directory,
                       unsigned int   id,
                       unsigned int   zoneCount,
                       unsigned int   length,
                       IndexState   **statePtr)
  __attribute__((warn_unused_result));

/*****************************************************************************/
static INLINE FileIndexState *asFileIndexState(IndexState *state)
{
  return container_of(state, FileIndexState, state);
}

#endif // FILE_INDEX_STATE_H
