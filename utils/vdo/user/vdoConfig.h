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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoConfig.h#3 $
 */

#ifndef VDO_CONFIG_H
#define VDO_CONFIG_H

#include "uds.h"

#include "types.h"
#include "volumeGeometry.h"

// The VDOConfig structure is fully declared in types.h

/**
 * Format a physical layer to function as a new VDO. This function must be
 * called on a physical layer before a VDO can be loaded for the first time on a
 * given layer. Once a layer has been formatted, it can be loaded and shut down
 * repeatedly. If a new VDO is desired, this function should be called again.
 *
 * @param config            The configuration parameters for the VDO
 * @param indexConfig       The configuration parameters for the index
 * @param layer             The physical layer the VDO will sit on
 *
 * @return VDO_SUCCESS or an error
 **/
int formatVDO(const VDOConfig *config,
              IndexConfig     *indexConfig,
              PhysicalLayer   *layer)
  __attribute__((warn_unused_result));


/**
 * Calculate minimal VDO based on config parameters.
 *
 * @param config        The VDOConfig to use for calculations
 * @param indexConfig   The IndexConfig to use for calculations
 * @param minVDOBlocks  A pointer to hold the minimum blocks needed
 * 
 * @return VDO_SUCCESS or error.
 **/
int calculateMinimumVDOFromConfig(const VDOConfig *config,
				  IndexConfig     *indexConfig,
				  BlockCount      *minVDOBlocks)
  __attribute__((warn_unused_result));
  
/**
 * Make a VDOLayout according to a VDOConfig. Exposed for testing only.
 *
 * @param [in]  config          The VDOConfig to generate a VDOLayout from
 * @param [in]  startingOffset  The start of the layouts
 * @param [out] vdoLayoutPtr    A pointer to hold the new VDOLayout
 *
 * @return VDO_SUCCESS or an error
 **/
int makeVDOLayoutFromConfig(const VDOConfig      *config,
                            PhysicalBlockNumber   startingOffset,
                            VDOLayout           **vdoLayoutPtr)
  __attribute__((warn_unused_result));

/**
 * This is a version of formatVDO() which allows the caller to supply the
 * desired VDO nonce and uuid. This function exists to facilitate unit tests
 * which attempt to ensure that version numbers are properly updated when
 * formats change.
 *
 * @param config            The configuration parameters for the VDO
 * @param indexConfig       The configuration parameters for the index
 * @param indexBlocks       Size of the index in blocks
 * @param layer             The physical layer the VDO will sit on
 * @param nonce             The nonce for the VDO
 * @param uuid              The uuid for the VDO
 *
 * @return VDO_SUCCESS or an error
 **/
int formatVDOWithNonce(const VDOConfig *config,
                       IndexConfig     *indexConfig,
                       PhysicalLayer   *layer,
                       Nonce            nonce,
                       UUID             uuid)
  __attribute__((warn_unused_result));

/**
 * Force the VDO to exit read-only mode and rebuild when it next loads
 * by setting the super block state.
 *
 * @param layer  The physical layer on which the VDO resides
 **/
int forceVDORebuild(PhysicalLayer *layer)
  __attribute__((warn_unused_result));

/**
 * Force the VDO to enter read-only mode when off-line.  This is only
 * used by a test utility.
 *
 * @param layer  The physical layer on which the VDO resides
 **/
int setVDOReadOnlyMode(PhysicalLayer *layer)
  __attribute__((warn_unused_result));

#endif /* VDO_CONFIG_H */
