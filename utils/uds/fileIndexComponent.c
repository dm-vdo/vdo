/*
 * Copyright (c) 2018 Red Hat, Inc.
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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/fileIndexComponent.c#1 $
 */

#include "fileIndexComponentInternal.h"

#include "compiler.h"
#include "errors.h"
#include "fileIORegion.h"
#include "fileUtils.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "stringUtils.h"
#include "typeDefs.h"

const mode_t DEFAULT_WRITE_MODE = 0777;

typedef struct fileReadPortal {
  ReadPortal common;
  PathBuffer path;
  size_t     plen;
} FileReadPortal;

typedef struct fileWriteZone {
  WriteZone  common;
  PathBuffer path;
} FileWriteZone;

/*****************************************************************************/
static INLINE FileReadPortal *asFileReadPortal(ReadPortal *portal)
{
  return container_of(portal, FileReadPortal, common);
}

/*****************************************************************************/
static INLINE FileWriteZone *asFileWriteZone(WriteZone *writeZone)
{
  return container_of(writeZone, FileWriteZone, common);
}

/*****************************************************************************/

static const IndexComponentOps *getFileIndexComponentOps(void);

/*****************************************************************************/
int makeFileIndexComponent(const IndexComponentInfo  *info,
                           const char                *readDir,
                           const char                *writeDir,
                           unsigned int               zoneCount,
                           void                      *data,
                           void                      *context,
                           IndexComponent           **componentPtr)
{
  if ((readDir == NULL) || (writeDir == NULL)) {
    return logErrorWithStringError(UDS_INVALID_ARGUMENT,
                                   "invalid read or write directory specified");
  }

  FileIndexComponent *fic = NULL;
  int result = ALLOCATE(1, FileIndexComponent, "file index component", &fic);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = initIndexComponent(&fic->common, info, zoneCount, data, context,
                              getFileIndexComponentOps());
  if (result != UDS_SUCCESS) {
    FREE(fic);
    return result;
  }

  static const char pathFormat[] = "%s/%s";

  result = initializePathBufferSprintf(&fic->readPath, pathFormat,
                                       readDir, info->fileName);
  if (result != UDS_SUCCESS) {
    destroyIndexComponent(&fic->common);
    FREE(fic);
    return result;
  }

  result = initializePathBufferSprintf(&fic->writePath, pathFormat,
                                       writeDir, info->fileName);
  if (result != UDS_SUCCESS) {
    releasePathBuffer(&fic->readPath);
    destroyIndexComponent(&fic->common);
    FREE(fic);
    return result;
  }

  *componentPtr = &fic->common;
  return UDS_SUCCESS;
}

/*****************************************************************************/
int makeLastComponentSaveReadable(IndexComponent *component)
{
  FileIndexComponent *fic = asFileIndexComponent(component);

  return loggingRename(pathBufferPath(&fic->writePath),
                       pathBufferPath(&fic->readPath),
                       __func__);
}

/**
 * Construct an array of ReadPortal instances, one for each
 * zone which exists in the directory.
 *
 * @param [in]  component      the index compoennt,
 * @param [in]  region         if non-NULL, the region for the existing
 *                             non-multi-zone file
 * @param [out] readPortalPtr  where to store the resulting array
 *
 * @return UDS_SUCCESS or an error code
 **/
static int makeFileReadPortal(IndexComponent  *component,
                              IORegion        *region,
                              FileReadPortal **frpPtr)
{
  FileIndexComponent *fic              = asFileIndexComponent(component);
  unsigned int        numZones         = 0;
  size_t              sizeOfCommonPart = 0;
  int                 result           = UDS_SUCCESS;

  PathBuffer zfile;     // path buffer used to lookup zone files

  if (region != NULL) {
    // compatibility: treat single file as a directory containing one zone
    result = initializePathBufferCopy(&zfile, &fic->readPath);
    if (result != UDS_SUCCESS) {
      return result;
    }
    numZones = 1;
  } else {
    // determine how many contiguous zone files exist
    result = initializePathBufferSizedSprintf(&zfile,
      pathBufferSize(&fic->readPath) + sizeof("/zoneXXXX"),
      "%s/zone", pathBufferPath(&fic->readPath));
    if (result != UDS_SUCCESS) {
      return result;
    }

    sizeOfCommonPart = pathBufferLength(&zfile);

    for (numZones = 0; ; ++numZones) {
      result = truncatePathBuffer(&zfile, sizeOfCommonPart);
      if (result != UDS_SUCCESS) {
        return result;
      }
      result = appendPathBufferSprintf(&zfile, "%u", numZones);
      if (result != UDS_SUCCESS) {
        return result;
      }

      bool exists = false;
      result = fileExists(pathBufferPath(&zfile), &exists);
      if (result != UDS_SUCCESS) {
        return result;
      }

      if (!exists) {
        break;
      }
    }
  }

  FileReadPortal *frp = NULL;
  result = ALLOCATE(1, FileReadPortal, "file read portal", &frp);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = initReadPortal(&frp->common, component, numZones);
  if (result != UDS_SUCCESS) {
    FREE(frp);
    return result;
  }

  // transfer ownership of zfile pathbuffer into readzone
  memcpy(&frp->path, &zfile, sizeof(PathBuffer));
  memset(&zfile, 0, sizeof(PathBuffer));
  frp->plen = sizeOfCommonPart;

  if (region != NULL) {
    frp->common.regions[0] = region;
  }

  *frpPtr = frp;
  return UDS_SUCCESS;
}

/**
 * Verify the existance of the multi-zone state directory.
 *
 * @param component    the index component
 *
 * @return UDS_SUCCESS       if the component's directory exists
 *         UDS_NO_DIRECTORY  if a file exists in the directory's place
 *         some other error code
 **/
static int checkStateDir(const FileIndexComponent *fic)
{
  const char *path = pathBufferPath(&fic->readPath);
  bool isDir = false;
  int result = isDirectory(path, &isDir);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result,
                                   "expected multi-zone directory %s", path);
  } else if (!isDir) {
    logInfo("using unzoned file %s", path);
    return UDS_NO_DIRECTORY;
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int openReadPortalRegions(FileReadPortal *frp)
{
  for (unsigned int z = 0; z < frp->common.zones; ++z) {
    if (frp->common.regions[z] != NULL) {
      continue;
    }
    int result = truncatePathBuffer(&frp->path, frp->plen);
    if (result != UDS_SUCCESS) {
      return result;
    }
    result = appendPathBufferSprintf(&frp->path, "%u", z);
    if (result != UDS_SUCCESS) {
      return result;
    }
    result = openFileRegion(pathBufferPath(&frp->path), FU_READ_ONLY,
                            &frp->common.regions[z]);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int makeFileWriteZone(FileIndexComponent  *fic,
                             unsigned int         zone,
                             size_t               pathSize,
                             WriteZone          **wzPtr)
{
  FileWriteZone *fwz = NULL;
  int result = ALLOCATE(1, FileWriteZone, "file write zone", &fwz);
  if (result != UDS_SUCCESS) {
    return result;
  }

  fwz->common = (WriteZone) {
    .component = &fic->common,
    .phase     = IWC_IDLE,
    .region    = NULL,
    .zone      = zone,
  };

  if (fic->common.info->multiZone) {
    result = initializePathBufferSizedSprintf(&fwz->path, pathSize, "%s/zone%u",
                                              pathBufferPath(&fic->writePath),
                                              zone);
  } else {
    result = initializePathBufferCopy(&fwz->path, &fic->writePath);
  }
  if (result != UDS_SUCCESS) {
    FREE(fwz);
    return result;
  }

  *wzPtr = &fwz->common;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static void fic_freeIndexComponent(IndexComponent *component)
{
  if (component == NULL) {
    return;
  }
  FileIndexComponent *fic = asFileIndexComponent(component);
  releasePathBuffer(&fic->readPath);
  releasePathBuffer(&fic->writePath);
  destroyIndexComponent(component);
  FREE(fic);
}

/*****************************************************************************/
static int fic_openWriteRegion(WriteZone *writeZone)
{
  FileWriteZone *fwz = asFileWriteZone(writeZone);

  return openFileRegion(pathBufferPath(&fwz->path),
                        FU_CREATE_READ_WRITE,
                        &writeZone->region);
}

/*****************************************************************************/
static int fic_cleanupWriteFailure(IndexComponent *component)
{
  FileIndexComponent *fic = asFileIndexComponent(component);

  if (component->info->multiZone) {
    return removeDirectory(pathBufferPath(&fic->writePath),
                           component->info->name);
  } else {
    return removeFile(pathBufferPath(&fic->writePath));
  }
}

/*****************************************************************************/
static void freeFileWriteZone(WriteZone *writeZone)
{
  FileWriteZone *fwz = asFileWriteZone(writeZone);
  releasePathBuffer(&fwz->path);
  FREE(fwz);
}

/*****************************************************************************/
static void fic_freeWriteZones(IndexComponent *component)
{
  freeWriteZonesHelper(component, freeFileWriteZone);
}

/*****************************************************************************/
static int fic_populateWriteZones(IndexComponent *component)
{
  FileIndexComponent *fic = asFileIndexComponent(component);

  size_t size = pathBufferLength(&fic->writePath) + sizeof("/zoneXXXX");

  for (unsigned int z = 0; z < component->numZones; ++z) {
    int result = makeFileWriteZone(fic, z, size, &component->writeZones[z]);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  return UDS_SUCCESS;
}

/*****************************************************************************/
static int fic_prepareZones(IndexComponent *component,
                            unsigned int    numZones __attribute__((unused)))
{
  FileIndexComponent *fic = asFileIndexComponent(component);

  if (component->info->multiZone) {
    return makeDirectory(pathBufferPath(&fic->writePath),
                         DEFAULT_WRITE_MODE,
                         "index component multi-zone",
                         __func__);
  }
  return UDS_SUCCESS;
}

/*****************************************************************************/
static void fic_freeReadPortal(ReadPortal *portal)
{
  if (portal == NULL) {
    return;
  }
  FileReadPortal *frp = asFileReadPortal(portal);
  for (unsigned int z = 0; z < portal->zones; ++z) {
    releasePathBuffer(&frp->path);
  }
  destroyReadPortal(portal);
  FREE(frp);
}

/*****************************************************************************/
static int fic_createReadPortal(IndexComponent  *component,
                                ReadPortal     **portalPtr)
{
  FileIndexComponent *fic = asFileIndexComponent(component);
  IORegion *region = NULL;
  int       result = UDS_NO_DIRECTORY;
  if (component->info->multiZone) {
    result = checkStateDir(fic);
  }
  const char *path = pathBufferPath(&fic->readPath);
  if (result == UDS_NO_DIRECTORY) {
    result = openFileRegion(path, FU_READ_ONLY, &region);
  }
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "failed to open %s", path);
  }
  FileReadPortal *frp = NULL;
  result = makeFileReadPortal(component, region, &frp);
  if (result != UDS_SUCCESS) {
    return result;
  }
  result = openReadPortalRegions(frp);
  if (result != UDS_SUCCESS) {
    fic_freeReadPortal(&frp->common);
    return result;
  }

  *portalPtr = &frp->common;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int fic_discardIndexComponent(IndexComponent *component)
{
  if (component == NULL) {
    return UDS_SUCCESS;
  }

  FileIndexComponent *fic = asFileIndexComponent(component);
  int result = UDS_SUCCESS;

  if (component->info->multiZone) {
    result = removeDirectory(pathBufferPath(&fic->readPath),
                             "multi-zone component read dir");
    int r = removeDirectory(pathBufferPath(&fic->writePath),
                            "multi-zone component write dir");
    result = firstError(result, r);
  } else {
    result = removeFile(pathBufferPath(&fic->readPath));
    result = firstError(result, removeFile(pathBufferPath(&fic->writePath)));
  }
  return result;
}

/*****************************************************************************/

static const IndexComponentOps fileIndexComponentOps = {
  .freeMe           = fic_freeIndexComponent,
  .openWriteRegion  = fic_openWriteRegion,
  .cleanupWrite     = fic_cleanupWriteFailure,
  .populateZones    = fic_populateWriteZones,
  .freeZones        = fic_freeWriteZones,
  .prepareZones     = fic_prepareZones,
  .createReadPortal = fic_createReadPortal,
  .freeReadPortal   = fic_freeReadPortal,
  .discard          = fic_discardIndexComponent,
};

static const IndexComponentOps *getFileIndexComponentOps(void)
{
  return &fileIndexComponentOps;
}
