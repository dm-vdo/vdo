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
 * $Id: //eng/uds-releases/jasper/src/uds/volumeStore.c#1 $
 */

#include "indexLayout.h"
#include "logger.h"
#include "uds-error.h"
#include "volumeStore.h"

/*****************************************************************************/
void closeVolumeStore(struct volume_store *volumeStore)
{
#ifdef __KERNEL__
  if (volumeStore->vs_client != NULL) {
    dm_bufio_client_destroy(volumeStore->vs_client);
    volumeStore->vs_client = NULL;
  }
#else
  if (volumeStore->vs_region != NULL) {
    putIORegion(volumeStore->vs_region);
    volumeStore->vs_region = NULL;
  }
#endif
}

/*****************************************************************************/
int openVolumeStore(struct volume_store *volumeStore,
                    IndexLayout  *layout,
                    unsigned int  reservedBuffers __attribute__((unused)),
                    size_t        bytesPerPage)
{
#ifdef __KERNEL__
  return openVolumeBufio(layout, bytesPerPage, reservedBuffers,
                         &volumeStore->vs_client);
#else
  volumeStore->vs_bytesPerPage = bytesPerPage;
  return openVolumeRegion(layout, &volumeStore->vs_region);
#endif
}

/*****************************************************************************/
void prefetchVolumePages(const struct volume_store *vs __attribute__((unused)),
                         unsigned int physicalPage __attribute__((unused)),
                         unsigned int pageCount __attribute__((unused)))
{
#ifdef __KERNEL__
  dm_bufio_prefetch(vs->vs_client, physicalPage, pageCount);
#else
  // Nothing to do in user mode
#endif
}

/*****************************************************************************/
int syncVolumeStore(const struct volume_store *volumeStore)
{
#ifdef __KERNEL__
  int result = -dm_bufio_write_dirty_buffers(volumeStore->vs_client);
#else
  int result = syncRegionContents(volumeStore->vs_region);
#endif
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "cannot sync chapter to volume");
  }
  return UDS_SUCCESS;
}
