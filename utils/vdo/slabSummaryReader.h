/*
 * Copyright Red Hat
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
 * $Id: //eng/vdo-releases/sulfur-rhel9.0-beta/src/c++/vdo/user/slabSummaryReader.h#1 $
 */

#ifndef SLAB_SUMMARY_READER_H
#define SLAB_SUMMARY_READER_H

#include "slabSummaryFormat.h"
#include "types.h"

#include "userVDO.h"

/**
 * Read the contents of the slab summary into a single set of summary entries.
 *
 * @param [in]  vdo          The vdo from which to read the summary
 * @param [out] entries_ptr  A pointer to hold the loaded entries
 *
 * @return VDO_SUCCESS or an error code
 **/
int __must_check
readSlabSummary(UserVDO *vdo, struct slab_summary_entry **entriesPtr);

#endif // SLAB_SUMMARY_UTILS_H
