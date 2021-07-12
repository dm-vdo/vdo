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
 * $Id: //eng/uds-releases/jasper/src/uds/convertToLVM.c#14 $
 */

#include "config.h"
#include "errors.h"
#include "index.h"
#include "indexConfig.h"
#include "indexLayout.h"
#include "indexRouter.h"
#include "indexSession.h"
#include "logger.h"
#include "uds.h"
#include "volume.h"
#include "volumeStore.h"

#include "convertToLVM.h"

/**
 * Move the data for physical chapter 0 to a new physical location.
 *
 * @param volume       The volume
 * @param layout       The index layout
 * @param newPhysical  The new physical chapter number to move to
 *
 * @return UDS_SUCCESS or an error code
 **/
__attribute__((warn_unused_result))
static int moveChapter(Volume      *volume,
                       IndexLayout *layout,
                       uint64_t     newPhysical)
{
  Geometry *geometry = volume->geometry;
  struct volume_store vs;
  int result = UDS_SUCCESS;
  unsigned int page;
  result = openVolumeStore(&vs, layout, 0, geometry->bytesPerPage);
  if (result != UDS_SUCCESS) {
    return result;
  }
  for (page = 0; page < geometry->pagesPerChapter; page++) {
    struct volume_page vp;
    unsigned int physicalPage =
      mapToPhysicalPage(geometry, 0, page);
    result = initializeVolumePage(geometry, &vp);
    if (result != UDS_SUCCESS) {
      return result;
    }
    result = readVolumePage(&vs, physicalPage, &vp);
    if (result != UDS_SUCCESS) {
      return result;
    }
    physicalPage =
      mapToPhysicalPage(geometry, newPhysical, page);
    result = writeVolumePage(&vs, physicalPage, &vp);
    if (result != UDS_SUCCESS) {
      return result;
    }
    destroyVolumePage(&vp);
  }
  closeVolumeStore(&vs);
  return UDS_SUCCESS;
}

/**
 * Destroy the index session after an error.
 *
 * @param session  The index session
 **/
static void cleanupSession(struct uds_index_session *session)
{
  if (session != NULL) {
    // This can produce an error when the index is not open.
    int result = udsCloseIndex(session);
    if (result != UDS_SUCCESS) {
      logWarningWithStringError(result, "Error closing index");
    }
    result = udsDestroyIndexSession(session);
    if (result != UDS_SUCCESS) {
      logWarningWithStringError(result, "Error closing index");
    }
  }
}

/**
 * Copy the index page map entries corresponding to physical chapter 0 to a new
 * location if necessary, and then shift the array of entries down to eliminate
 * the old entries for physical chapter 0. When saving the page map, the end of
 * the entries array will be ignored.
 *
 * @param volume       The volume
 * @param newPhysical  The new physical chapter slot to move to
 *
 * @return UDS_SUCCESS or an error code
 **/
static int reduceIndexPageMap(Volume *volume, uint64_t newPhysical)
{
  IndexPageMap *map = volume->indexPageMap;
  Geometry *geometry = volume->geometry;
  int entriesPerChapter = geometry->indexPagesPerChapter - 1;
  int reducedEntries = (geometry->chaptersPerVolume - 1) * entriesPerChapter;

  // Copy slot entries for the moved chapter to the new location.
  if (newPhysical > 0) {
    size_t slot = newPhysical * entriesPerChapter;
    size_t chapterSlotSize = sizeof(IndexPageMapEntry) * entriesPerChapter;
    memcpy(&map->entries[slot], map->entries, chapterSlotSize);
  }

  // Shift the entries down to match the new set of chapters.
  memmove(map->entries, &map->entries[entriesPerChapter],
          reducedEntries * sizeof(IndexPageMapEntry));

  return UDS_SUCCESS;
}

/**********************************************************************/
int udsConvertToLVM(const char       *name,
                    size_t            freedSpace,
                    UdsConfiguration  config,
                    off_t            *chapterSize)
{
  struct uds_index_session *session = NULL;
  struct uds_parameters parameters = UDS_PARAMETERS_INITIALIZER;
  UdsConfiguration userConfig;
  IndexRouter *router;
  Index *index;
  IndexLayout *layout;
  Volume *volume;
  uint64_t oldest;
  uint64_t newest;
  unsigned int chaptersPerVolume;
  uint64_t remappedVirtual;
  uint64_t newPhysical;
  Geometry *geometry;

  int result = udsCreateIndexSession(&session);
  if (result != UDS_SUCCESS) {
    return result;
  }
  parameters.zone_count = 1;
  result = udsOpenIndex(UDS_NO_REBUILD,
                        name,
                        &parameters,
                        config,
                        session);
  if (result != UDS_SUCCESS) {
    cleanupSession(session);
    return result;
  }

  userConfig = &session->userConfig;
  router = session->router;
  index = router->index;
  layout = index->layout;
  volume = index->volume;
  oldest = index->oldestVirtualChapter;
  newest = index->newestVirtualChapter;
  chaptersPerVolume = userConfig->chaptersPerVolume;

  result = ASSERT((freedSpace <= volume->geometry->bytesPerChapter),
                  "cannot free more than %zu bytes (%zu requested)",
                  volume->geometry->bytesPerChapter, freedSpace);
  if (result != UDS_SUCCESS) {
    cleanupSession(session);
    return result;
  }

  logInfo("index has chapters %llu to %llu\n",
          (long long) oldest,
          (long long) newest);

  if (newest - oldest > chaptersPerVolume - 2) {
    result = forgetChapter(volume,
                           oldest,
                           INVALIDATION_EXPIRE);
    if (result != UDS_SUCCESS) {
      cleanupSession(session);
      return result;
    }
    index->oldestVirtualChapter++;
  }

  // Remap the chapter currently in physical chapter 0.
  remappedVirtual = newest - (newest % chaptersPerVolume);
  newPhysical = (newest + 1) % chaptersPerVolume;

  result = reduceIndexPageMap(volume, newPhysical);
  if (result != UDS_SUCCESS) {
    cleanupSession(session);
    return result;
  }

  if (newPhysical == 0) {
    /*
     * We've already expired the oldest chapter. But pretend we
     * moved the next virtual chapter to where it should go.
     * This simplifies the virtual to physical mapping math.
     */
    remappedVirtual += chaptersPerVolume;
    newPhysical = 1;
  } else {
    // The open chapter has no state in the volume to move.
    result = moveChapter(volume, layout, newPhysical);
    if (result != UDS_SUCCESS) {
      cleanupSession(session);
      return result;
    }
  }

  userConfig->remappedVirtual  = remappedVirtual;
  userConfig->remappedPhysical = newPhysical - 1;
  userConfig->chaptersPerVolume = chaptersPerVolume - 1;

  result = makeGeometry(userConfig->bytesPerPage,
                        userConfig->recordPagesPerChapter,
                        userConfig->chaptersPerVolume,
                        userConfig->sparseChaptersPerVolume,
                        userConfig->remappedVirtual,
                        userConfig->remappedPhysical,
                        &geometry);
  if (result != UDS_SUCCESS) {
    cleanupSession(session);
    return result;
  }

  *volume->geometry = *geometry;
  freeGeometry(geometry);

  logDebug("Saving updated layout and writing index configuration");
  result = updateLayout(layout, userConfig, freedSpace,
                        volume->geometry->bytesPerChapter);
  if (result != UDS_SUCCESS) {
    cleanupSession(session);
    return result;
  }

  *config = *userConfig;
  *chapterSize = volume->geometry->bytesPerChapter;

  // Force a save, even though no new requests have been processed, so
  // that the save areas get updated
  router->needToSave = true;
  cleanupSession(session);
  return UDS_SUCCESS;
}
