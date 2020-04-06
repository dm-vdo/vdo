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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/indexLayoutLinuxUser.c#2 $
 */

#include "errors.h"
#include "indexLayout.h"
#include "indexLayoutParser.h"
#include "ioFactory.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "uds.h"

/*****************************************************************************/
int makeIndexLayout(const char *name,
		    bool newLayout,
		    const UdsConfiguration config,
		    IndexLayout **layoutPtr)
{
	char *file = NULL;
	uint64_t offset = 0;
	uint64_t size = 0;

	LayoutParameter parameterTable[] = {
		{ "file", LP_STRING | LP_DEFAULT, { .str = &file }, false },
		{ "size", LP_UINT64, { .num = &size }, false },
		{ "offset", LP_UINT64, { .num = &offset }, false },
	};

	char *params = NULL;
	int result =
		duplicateString(name, "makeIndexLayout parameters", &params);
	if (result != UDS_SUCCESS) {
		return result;
	}

	// note file will be set to memory owned by params
	result = parseLayoutString(
		params, parameterTable, COUNT_OF(parameterTable));
	if (result != UDS_SUCCESS) {
		FREE(params);
		return result;
	}

	if (!file) {
		FREE(params);
		return logErrorWithStringError(UDS_INDEX_NAME_REQUIRED,
					       "no index specified");
	}

	IOFactory *factory = NULL;
	result = makeIOFactory(file,
			       newLayout ? FU_CREATE_READ_WRITE : FU_READ_WRITE,
			       &factory);
	FREE(params);
	if (result != UDS_SUCCESS) {
		return result;
	}
	IndexLayout *layout;
	result = makeIndexLayoutFromFactory(
		factory, offset, size, newLayout, config, &layout);
	putIOFactory(factory);
	if (result != UDS_SUCCESS) {
		return result;
	}
	*layoutPtr = layout;
	return UDS_SUCCESS;
}
