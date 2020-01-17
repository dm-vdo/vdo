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
 * $Id: //eng/linux-vdo/src/c++/vdo/base/compressionStateInternals.h#3 $
 */

#ifndef COMPRESSION_STATE_INTERNALS_H
#define COMPRESSION_STATE_INTERNALS_H

#include "compressionState.h"

/**
 * Set the compression state of a data_vio (exposed for testing).
 *
 * @param dataVIO   The data_vio whose compression state is to be set
 * @param state     The expected current state of the data_vio
 * @param newState  The state to set
 *
 * @return <code>true</code> if the new state was set, false if the data_vio's
 *         compression state did not match the expected state, and so was
 *         left unchanged
 **/
bool setCompressionState(struct data_vio              *dataVIO,
                         struct vio_compression_state  state,
                         struct vio_compression_state  newState);

#endif /* COMPRESSION_STATE_H */
