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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoVolumeUtils.c#24 $
 */

#include "vdoVolumeUtils.h"

#include <err.h>

#include "constants.h"
#include "fixedLayout.h"
#include "slab.h"
#include "slabDepotInternals.h"
#include "slabSummaryInternals.h"
#include "types.h"
#include "vdoComponentStates.h"
#include "vdoDecode.h"
#include "vdoInternal.h"
#include "vdoLayout.h"

#include "fileLayer.h"

static char errBuf[ERRBUF_SIZE];

/**********************************************************************/
static int __must_check
decode_vdo(struct vdo *vdo, bool validate_config)
{
	int result = start_vdo_decode(vdo, validate_config);
	if (result != VDO_SUCCESS) {
		destroy_component_states(&vdo->states);
		return result;
	}

	result = decode_vdo_layout(vdo->states.layout, &vdo->layout);
	if (result != VDO_SUCCESS) {
		destroy_component_states(&vdo->states);
		return result;
	}
	return finish_vdo_decode(vdo);
}

/**********************************************************************/
int load_vdo_superblock(PhysicalLayer *layer,
                        struct volume_geometry *geometry,
                        bool validate_config,
                        struct vdo **vdo_ptr)
{
	struct vdo *vdo;
	int result = make_vdo(layer, &vdo);
	if (result != VDO_SUCCESS) {
		return result;
	}

	set_load_config_from_geometry(geometry, &vdo->load_config);
	result = load_super_block(layer, get_first_block_offset(vdo),
				  &vdo->super_block);
	if (result != VDO_SUCCESS) {
		free_vdo(&vdo);
		return result;
	}

	result = decode_vdo(vdo, validate_config);
	if (result != VDO_SUCCESS) {
		free_vdo(&vdo);
		return result;
	}

	*vdo_ptr = vdo;
	return VDO_SUCCESS;
}

/**********************************************************************/
int load_vdo(PhysicalLayer *layer,
	     bool validate_config,
	     struct vdo **vdo_ptr)
{
	struct volume_geometry geometry;
	int result = load_volume_geometry(layer, &geometry);
	if (result != VDO_SUCCESS) {
		return result;
	}

	return load_vdo_superblock(layer, &geometry, validate_config, vdo_ptr);
}

/**
 * Load a VDO from a file.
 *
 * @param [in]  filename        The file name
 * @param [in]  readOnly        Whether the layer should be read-only.
 * @param [in]  validateConfig  Whether the VDO should validate its config
 * @param [out] vdoPtr          A pointer to hold the VDO
 *
 * @return VDO_SUCCESS or an error code
 **/
static int __must_check loadVDOFromFile(const char *filename,
					bool readOnly,
					bool validateConfig,
					UserVDO **vdoPtr)
{
  int result = ASSERT(validateConfig || readOnly,
                      "Cannot make a writable VDO"
                      " without validating its config");
  if (result != UDS_SUCCESS) {
    return result;
  }

  PhysicalLayer *layer;
  if (readOnly) {
    result = makeReadOnlyFileLayer(filename, &layer);
  } else {
    result = makeFileLayer(filename, 0, &layer);
  }

  if (result != VDO_SUCCESS) {
    warnx("Failed to make FileLayer from '%s' with %s",
          filename, stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  // Create the VDO.
  UserVDO *vdo;
  result = makeUserVDO(layer, &vdo);
  if (result != VDO_SUCCESS) {
    layer->destroy(&layer);
    warnx("makeUserVDO failed with %s",
          stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  result = load_volume_geometry(layer, &vdo->geometry);
  if (result != VDO_SUCCESS) {
    layer->destroy(&layer);
    warnx("load_volume_geometry failed with %s",
          stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  result = load_vdo_superblock(layer, &vdo->geometry, validateConfig,
                               &vdo->vdo);
  if (result != VDO_SUCCESS) {
    layer->destroy(&layer);
    warnx("allocateVDO failed for '%s' with %s",
          filename, stringError(result, errBuf, ERRBUF_SIZE));
    return result;
  }

  vdo->states = vdo->vdo->states;
  setDerivedSlabParameters(vdo);
  *vdoPtr = vdo;
  return VDO_SUCCESS;
}

/**********************************************************************/
int makeVDOFromFile(const char *filename, bool readOnly, UserVDO **vdoPtr)
{
  return loadVDOFromFile(filename, readOnly, true, vdoPtr);
}

/**********************************************************************/
int readVDOWithoutValidation(const char *filename, UserVDO **vdoPtr)
{
  return loadVDOFromFile(filename, true, false, vdoPtr);
}

/**********************************************************************/
void freeVDOFromFile(UserVDO **vdoPtr)
{
  UserVDO *vdo = *vdoPtr;
  if (vdo == NULL) {
    return;
  }

  free_vdo(&vdo->vdo);
  vdo->layer->destroy(&vdo->layer);
  freeUserVDO(&vdo);

  *vdoPtr = NULL;
}
