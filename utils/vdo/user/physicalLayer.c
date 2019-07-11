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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/physicalLayer.c#11 $
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
void destroy_vio(VIO **vioPtr __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void submitMetadataVIO(VIO *vio __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void hashDataVIO(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void checkForDuplication(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void verifyDuplication(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void updateDedupeIndex(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void zeroDataVIO(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void copyData(DataVIO *source      __attribute__((unused)),
              DataVIO *destination __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void applyPartialWrite(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void acknowledgeDataVIO(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void compressDataVIO(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void writeDataVIO(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void readDataVIO(DataVIO *dataVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
void writeCompressedBlock(AllocatingVIO *allocatingVIO __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
}

/**********************************************************************/
bool compareDataVIOs(DataVIO *first  __attribute__((unused)),
		     DataVIO *second __attribute__((unused)))
{
  ASSERT_LOG_ONLY(false, "Stubs are never called");
  return false;
}
