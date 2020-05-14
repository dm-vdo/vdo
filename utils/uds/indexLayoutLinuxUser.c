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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/indexLayoutLinuxUser.c#7 $
 */

#include "errors.h"
#include "indexLayout.h"
#include "indexLayoutParser.h"
#include "ioFactory.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "uds.h"

/*****************************************************************************/
int make_index_layout(const char *name,
		      bool new_layout,
		      const struct uds_configuration *config,
		      struct index_layout **layout_ptr)
{
	char *file = NULL;
	uint64_t offset = 0;
	uint64_t size = 0;

	struct layout_parameter parameter_table[] = {
		{ "file", LP_STRING | LP_DEFAULT, { .str = &file }, false },
		{ "size", LP_UINT64, { .num = &size }, false },
		{ "offset", LP_UINT64, { .num = &offset }, false },
	};

	char *params = NULL;
	int result =
		duplicateString(name, "make_index_layout parameters", &params);
	if (result != UDS_SUCCESS) {
		return result;
	}

	// note file will be set to memory owned by params
	result = parse_layout_string(params,
				     parameter_table,
				     COUNT_OF(parameter_table));
	if (result != UDS_SUCCESS) {
		FREE(params);
		return result;
	}

	if (!file) {
		FREE(params);
		return logErrorWithStringError(UDS_INDEX_NAME_REQUIRED,
					       "no index specified");
	}

	struct io_factory *factory = NULL;
	result =
		make_io_factory(file,
			      new_layout ? FU_CREATE_READ_WRITE : FU_READ_WRITE,
			      &factory);
	FREE(params);
	if (result != UDS_SUCCESS) {
		return result;
	}
	struct index_layout *layout;
	result = make_index_layout_from_factory(
		factory, offset, size, new_layout, config, &layout);
	put_io_factory(factory);
	if (result != UDS_SUCCESS) {
		return result;
	}
	*layout_ptr = layout;
	return UDS_SUCCESS;
}
