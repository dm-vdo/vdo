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
 * $Id: //eng/uds-releases/flanders-rhel7.5/src/uds/multiFileLayout.c#1 $
 */

#include "multiFileLayoutInternals.h"

#include "config.h"
#include "fileBufferedReader.h"
#include "fileIORegion.h"
#include "fileIndexState.h"
#include "fileUtils.h"
#include "indexLayout.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "stringUtils.h"

/*****************************************************************************/
/**
 * Check that the index directory exists.
 *
 * @param [in]  path    The index directory path.
 * @param [out] exists  Whether the directory exists or not.
 *
 * @param UDS_SUCCESS or an error code.
 **/
__attribute__((warn_unused_result))
static int checkIndexDirectoryExists(const char *path, bool *exists)
{
  bool isDir;
  int result = isDirectory(path, &isDir);
  if (result == ENOENT) {
    *exists = false;
    return UDS_SUCCESS;
  }
  if (result != UDS_SUCCESS) {
    return result;
  }
  if (!isDir) {
    return logErrorWithStringError(UDS_INDEX_PATH_NOT_DIR,
                                   "index root path, %s, exists but "
                                   "isn't a directory",
                                   path);
  }
  *exists = true;
  return UDS_SUCCESS;
}

/*****************************************************************************/
int makeMultiFileLayout(const char *path, IndexLayout **layoutPtr)
{
  MultiFileLayout *mfl = NULL;
  int result = ALLOCATE(1, MultiFileLayout, "multi file layout", &mfl);
  if (result != UDS_SUCCESS) {
    return result;
  }
  zeroPathBuffer(&mfl->volumePath);

  result = initializeMultiFileLayout(mfl, path, NULL, NULL);
  if (result != UDS_SUCCESS) {
    destroyMultiFileLayout(mfl);
    FREE(mfl);
    return result;
  }

  *layoutPtr = &mfl->common;
  return UDS_SUCCESS;
}

/*****************************************************************************/
void destroyMultiFileLayout(MultiFileLayout *mfl)
{
  if (mfl != NULL) {
    if (mfl->cleanupFunc != NULL) {
      mfl->cleanupFunc(mfl);
    }
    releasePathBuffer(&mfl->volumePath);
    FREE(mfl->configFilename);
    FREE(mfl->sealFilename);
    FREE(mfl->indexDir);
  }
}

/*****************************************************************************/
static void mfl_freeMFL(IndexLayout *layout)
{
  if (layout) {
    MultiFileLayout *mfl = asMultiFileLayout(layout);
    destroyMultiFileLayout(mfl);
    FREE(mfl);
  }
}

/*****************************************************************************/
static int mfl_writeSeal(IndexLayout *layout)
{
  MultiFileLayout *mfl = asMultiFileLayout(layout);

  int fd;
  int result = openFile(mfl->sealFilename, FU_CREATE_READ_WRITE, &fd);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "failed to write safety seal file");
  }
  result = writeBuffer(fd, "*", 1);
  if (result != UDS_SUCCESS) {
    tryCloseFile(fd);
    return logErrorWithStringError(result, "failed to write safety seal file");
  }
  return syncAndCloseFile(fd, "failed to close safety seal file");
}

/*****************************************************************************/
static int mfl_removeSeal(IndexLayout *layout)
{
  MultiFileLayout *mfl = asMultiFileLayout(layout);

  return removeFile(mfl->sealFilename);
}

/*****************************************************************************/
static int mfl_checkIndexExists(IndexLayout *layout, bool *exists)
{
  MultiFileLayout *mfl = asMultiFileLayout(layout);

  return fileExists(mfl->configFilename, exists);
}

/*****************************************************************************/
static int mfl_checkSealed(IndexLayout *layout, bool *sealed)
{
  bool exists = false;
  int result = mfl_checkIndexExists(layout, &exists);
  if (result != UDS_SUCCESS) {
    return result;
  }
  if (!exists) {
    return UDS_NO_INDEX;
  }

  MultiFileLayout *mfl = asMultiFileLayout(layout);
  return fileExists(mfl->sealFilename, sealed);
}

/*****************************************************************************/
static int mfl_writeConfig(IndexLayout *layout, UdsConfiguration config)
{
  MultiFileLayout *mfl = asMultiFileLayout(layout);

  bool exists = false;
  int result = checkIndexDirectoryExists(mfl->indexDir, &exists);
  if (result != UDS_SUCCESS) {
    return result;
  }
  if (exists) {
    removeDirectory(mfl->indexDir, "index directory");
    result = checkIndexDirectoryExists(mfl->indexDir, &exists);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }
  if (exists) {
    result = isEmptyDirectory(mfl->indexDir, &exists);
    if (result != UDS_SUCCESS) {
      return result;
    }
    if (!exists) {
      logError("index root directory %s not empty", mfl->indexDir);
      return UDS_INDEX_EXISTS;
    }
  } else {
    result = makeDirectory(mfl->indexDir, 0777, "index root", __func__);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  IORegion *region;
  result = openFileRegion(mfl->configFilename, FU_CREATE_READ_WRITE, &region);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "failed to open config file");
  }
  if (mfl->fiddleRegion != NULL) {
    int result = mfl->fiddleRegion(region);
    if (result != UDS_SUCCESS) {
      closeIORegion(&region);
      return result;
    }
  }
  BufferedWriter *writer;
  result = makeBufferedWriter(region, 0, &writer);
  if (result != UDS_SUCCESS) {
    closeIORegion(&region);
    return logErrorWithStringError(result, "failed to get config writer");
  }
  result = writeConfigContents(writer, config);
  if (result != UDS_SUCCESS) {
    freeBufferedWriter(writer);
    closeIORegion(&region);
    return logErrorWithStringError(result, "failed to write config file");
  }
  result = flushBufferedWriter(writer);
  if (result != UDS_SUCCESS) {
    freeBufferedWriter(writer);
    closeIORegion(&region);
    return logErrorWithStringError(result, "cannot flush config writer");
  }
  freeBufferedWriter(writer);
  return syncAndCloseRegion(&region, "failed to close config file");
}

/*****************************************************************************/
static int mfl_readConfig(IndexLayout *layout, UdsConfiguration config)
{
  MultiFileLayout *mfl = asMultiFileLayout(layout);

  bool exists;
  int result = checkIndexExists(layout, &exists);
  if (result != UDS_SUCCESS) {
    return result;
  }
  if (!exists) {
    return UDS_NO_INDEX;
  }

  BufferedReader *reader;
  result = openFileBufferedReader(mfl->configFilename, &reader);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "cannot open index config file");
  }
  result = readConfigContents(reader, config);
  if (result != UDS_SUCCESS) {
    freeBufferedReader(reader);
    return logErrorWithStringError(result, "cannot read index config data");
  }
  freeBufferedReader(reader);
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int mfl_openVolumeRegion(IndexLayout   *layout,
                                unsigned int   indexId,
                                IOAccessMode   access,
                                IORegion     **regionPtr)
{
  MultiFileLayout *mfl = asMultiFileLayout(layout);

  int result = truncatePathBuffer(&mfl->volumePath, mfl->volumeCommon);
  if (result == UDS_SUCCESS) {
    result = appendPathBufferSprintf(&mfl->volumePath, "%u", indexId);
  }
  if ((result == UDS_SUCCESS) && (access & IO_CREATE)) {
    bool exists = true;
    result = checkIndexDirectoryExists(mfl->indexDir, &exists);
    if ((result == UDS_SUCCESS) && !exists) {
      result = makeDirectory(mfl->indexDir, 0777, "index root", __func__);
    }
  }
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "cannot open volume %u", indexId);
  }

  FileAccess fa = fileAccessMode(access);
  IORegion *region;
  result = openFileRegion(pathBufferPath(&mfl->volumePath), fa, &region);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "cannot open volume %u", indexId);
  }
  if (mfl->fiddleRegion != NULL) {
    int result = mfl->fiddleRegion(region);
    if (result != UDS_SUCCESS) {
      closeIORegion(&region);
      return result;
    }
  }
  *regionPtr = region;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static int mfl_makeIndexState(IndexLayout   *layout,
                              unsigned int   indexId,
                              unsigned int   numZones,
                              unsigned int   maxComponents,
                              IndexState   **statePtr)
{
  MultiFileLayout *mfl = asMultiFileLayout(layout);

  return makeFileIndexState(mfl->indexDir, indexId, numZones,
                            maxComponents, statePtr);
}

/*****************************************************************************/
static int mfl_getVolumeNonce(IndexLayout *layout  __attribute__((unused)),
                              unsigned int indexId __attribute__((unused)),
                              uint64_t    *nonce)
{
  *nonce = 0;
  return UDS_SUCCESS;
}

/*****************************************************************************/
int initializeMultiFileLayout(MultiFileLayout *mfl, const char *path,
                              void (*cleanupFunc)(MultiFileLayout *),
                              int (*fiddleRegion)(IORegion *))
{
  mfl->common.checkIndexExists = mfl_checkIndexExists;
  mfl->common.checkSealed      = mfl_checkSealed;
  mfl->common.free             = mfl_freeMFL;
  mfl->common.getVolumeNonce   = mfl_getVolumeNonce;
  mfl->common.makeIndexState   = mfl_makeIndexState;
  mfl->common.openVolumeRegion = mfl_openVolumeRegion;
  mfl->common.readConfig       = mfl_readConfig;
  mfl->common.removeSeal       = mfl_removeSeal;
  mfl->common.writeConfig      = mfl_writeConfig;
  mfl->common.writeSeal        = mfl_writeSeal;

  int result = duplicateString(path, "multi file layout index directory",
                               &mfl->indexDir);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = allocSprintf("multi file layout seal file",
                        &mfl->sealFilename, "%s/seal", mfl->indexDir);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = allocSprintf("multi file layout config file",
                        &mfl->configFilename, "%s/config", mfl->indexDir);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = initializePathBufferSprintf(&mfl->volumePath, "%s/volume_99999",
                                       mfl->indexDir);
  if (result != UDS_SUCCESS) {
    return result;
  }
  mfl->volumeCommon = pathBufferLength(&mfl->volumePath) - sizeof("99999") + 1;
  mfl->cleanupFunc  = cleanupFunc;
  mfl->fiddleRegion = fiddleRegion;
  return UDS_SUCCESS;
}
