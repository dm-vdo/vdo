/*
 * Copyright (c) 2019 Red Hat, Inc.
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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/physicalLayer.c#14 $
 */

#include "physicalLayer.h"

#include <zlib.h>

#include "permassert.h"

// Genuine implementations of certain physical layer functions, necessary
// for user tools.

/**********************************************************************/
CRC32Checksum update_crc32(CRC32Checksum  crc,
                           const byte    *buffer,
                           size_t         length)
{
  return crc32(crc, buffer, length);
}

// Stubs implementing the physical layer functions. They are unused by user
// tools, but still necessary to link against physical layer functions.

/**********************************************************************/
ThreadID getCallbackThreadID(void)
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
  return -1;
}

/**********************************************************************/
void destroy_vio(struct vio **vioPtr __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void submitMetadataVIO(struct vio *vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void hashDataVIO(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void checkForDuplication(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void verifyDuplication(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void updateDedupeIndex(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void zeroDataVIO(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void copyData(struct data_vio *source      __attribute__((unused)),
              struct data_vio *destination __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void applyPartialWrite(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void acknowledgeDataVIO(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void compressDataVIO(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void writeDataVIO(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void readDataVIO(struct data_vio *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void writeCompressedBlock(struct allocating_vio *allocatingVIO
                          __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
bool compareDataVIOs(struct data_vio *first  __attribute__((unused)),
		     struct data_vio *second __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
  return false;
}
