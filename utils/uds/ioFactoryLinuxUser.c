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
 * $Id: //eng/uds-releases/krusty/userLinux/uds/ioFactoryLinuxUser.c#1 $
 */

#include "atomicDefs.h"
#include "fileIORegion.h"
#include "ioFactory.h"
#include "memoryAlloc.h"

/*
 * A user mode IOFactory object controls access to an index stored in a file.
 */
struct ioFactory {
  int      fd;
  atomic_t refCount;
};

/*****************************************************************************/
void getIOFactory(IOFactory *factory)
{
  atomic_inc(&factory->refCount);
}

/*****************************************************************************/
int makeIOFactory(const char *path, FileAccess access, IOFactory **factoryPtr)
{
  IOFactory *factory;
  int result = ALLOCATE(1, IOFactory, __func__, &factory);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = openFile(path, access, &factory->fd);
  if (result != UDS_SUCCESS) {
    FREE(factory);
    return result;
  }

  atomic_set_release(&factory->refCount, 1);
  *factoryPtr = factory;
  return UDS_SUCCESS;
}

/*****************************************************************************/
void putIOFactory(IOFactory *factory)
{
  if (atomic_add_return(-1, &factory->refCount) <= 0) {
    closeFile(factory->fd, NULL);
    FREE(factory);
  }
}

/*****************************************************************************/
size_t getWritableSize(IOFactory *factory __attribute__((unused)))
{
  /*
   * The actual maximum is dependent upon the type of filesystem, and the man
   * pages tell us no way to determine what that maximum is.  Fortunately, any
   * attempt to write to a location that is too large will return an EFBIG
   * error.
   */ 
  return SIZE_MAX;
}

/*****************************************************************************/
int makeIORegion(IOFactory  *factory,
                 off_t       offset,
                 size_t      size,
                 IORegion  **regionPtr)
{
  return makeFileRegion(factory, factory->fd, FU_READ_WRITE, offset, size,
                        regionPtr);
}

/*****************************************************************************/
int openBufferedReader(IOFactory       *factory,
                       off_t            offset,
                       size_t           size,
                       BufferedReader **readerPtr)
{
  IORegion *region;
  int result = makeFileRegion(factory, factory->fd, FU_READ_WRITE, offset,
                              size, &region);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = makeBufferedReader(region, readerPtr);
  putIORegion(region);
  return result;
}

/*****************************************************************************/
int openBufferedWriter(IOFactory       *factory,
                       off_t            offset,
                       size_t           size,
                       BufferedWriter **writerPtr)
{
  IORegion *region;
  int result = makeFileRegion(factory, factory->fd, FU_READ_WRITE, offset,
                              size, &region);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = makeBufferedWriter(region, writerPtr);
  putIORegion(region);
  return result;
}
