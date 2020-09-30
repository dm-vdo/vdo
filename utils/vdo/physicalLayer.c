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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/physicalLayer.c#17 $
 */

#include "physicalLayer.h"

#include <zlib.h>

#include "permassert.h"

// Genuine implementations of certain physical layer functions, necessary
// for user tools.

/**********************************************************************/
crc32_checksum_t update_crc32(crc32_checksum_t  crc,
                              const byte       *buffer,
                              size_t            length)
{
  return crc32(crc, buffer, length);
}

// Stubs implementing the physical layer functions. They are unused by user
// tools, but still necessary to link against physical layer functions.

/**********************************************************************/
thread_id_t getCallbackThreadID(void)
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
  return -1;
}

/**********************************************************************/
void destroy_vio(struct vio **vio_ptr __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void submit_metadata_vio(struct vio *vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void hash_data_vio(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void check_for_duplication(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void verify_duplication(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void update_dedupe_index(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void zero_data_vio(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void copy_data(struct data_vio *source      __attribute__((unused)),
               struct data_vio *destination __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void apply_partial_write(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void acknowledge_data_vio(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void compress_data_vio(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void write_data_vio(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void read_data_vio(struct data_vio *data_vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void write_compressed_block(struct allocating_vio *allocating_vio
                            __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
bool compare_data_vios(struct data_vio *first  __attribute__((unused)),
		       struct data_vio *second __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
  return false;
}
