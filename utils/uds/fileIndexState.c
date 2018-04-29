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
 * $Id: //eng/uds-releases/flanders-rhel7.5/userLinux/uds/fileIndexState.c#1 $
 */

#include "fileIndexState.h"

#include "errors.h"
#include "fileIndexComponent.h"
#include "fileUtils.h"
#include "logger.h"
#include "memoryAlloc.h"
#include "stringUtils.h"

static const char CURRENT_DIR[]  = "current";
static const char NEXT_DIR[]     = "next";
static const char PREVIOUS_DIR[] = "previous";
static const char DELETION_DIR[] = "deletion";

static const IndexStateOps *getFileIndexStateOps(void);

/*****************************************************************************/
static void freeFileIndexState(FileIndexState *fis)
{
  if (fis == NULL) {
    return;
  }
  FREE(fis->deletion);
  FREE(fis->previous);
  FREE(fis->next);
  FREE(fis->current);
  destroyIndexState(&fis->state);
  FREE(fis);
}

/*****************************************************************************/
int makeFileIndexState(const char    *directory,
                       unsigned int   id,
                       unsigned int   zoneCount,
                       unsigned int   length,
                       IndexState   **statePtr)
{
  FileIndexState *fis = NULL;
  int result = ALLOCATE(1, FileIndexState, "file index state", &fis);
  if (result != UDS_SUCCESS) {
    return result;
  }

  result = initIndexState(&fis->state, id, zoneCount, length,
                          getFileIndexStateOps());
  if (result != UDS_SUCCESS) {
    FREE(fis);
    return result;
  }

  static const char pathFormat[] = "%s/%s_%u";

  result = allocSprintf("index state current directory path",
                        &fis->current, pathFormat,
                        directory, CURRENT_DIR, id);
  if (result != UDS_SUCCESS) {
    freeFileIndexState(fis);
    return result;
  }

  result = allocSprintf("index state next directory path",
                        &fis->next, pathFormat,
                        directory, NEXT_DIR, id);
  if (result != UDS_SUCCESS) {
    freeFileIndexState(fis);
    return result;
  }

  result = allocSprintf("index state previous directory path",
                        &fis->previous, pathFormat,
                        directory, PREVIOUS_DIR, id);
  if (result != UDS_SUCCESS) {
    freeFileIndexState(fis);
    return result;
  }

  result = allocSprintf("index state deletion directory path",
                        &fis->deletion, pathFormat,
                        directory, DELETION_DIR, id);
  if (result != UDS_SUCCESS) {
    freeFileIndexState(fis);
    return result;
  }

  *statePtr = &fis->state;
  return UDS_SUCCESS;
}

/*****************************************************************************/
static void fis_freeMe(IndexState *state)
{
  if (state != NULL) {
    freeFileIndexState(asFileIndexState(state));
  }
}

/*****************************************************************************/
static int fis_addComponent(IndexState               *state,
                            const IndexComponentInfo *info,
                            void                     *data,
                            void                     *context)
{
  FileIndexState *fis = asFileIndexState(state);

  IndexComponent *component = NULL;
  int result = makeFileIndexComponent(info, fis->current, fis->next,
                                      state->zoneCount, data, context,
                                      &component);
  if (result != UDS_SUCCESS) {
    return logErrorWithStringError(result, "makeIndexComponent() failed.");
  }

  result = addComponentToIndexState(&fis->state, component);
  if (result != UDS_SUCCESS) {
    freeIndexComponent(&component);
    return result;
  }
  return UDS_SUCCESS;
}

/**
 *  Determine if the specified path is a directory.
 *
 *  @param path         a file path
 *
 *  @return true if the specified path is a directory
 *          false otherwise (logs if exists but not directory)
 **/
static bool directoryExists(const char *path)
{
  bool isDir = false;
  int result = isDirectory(path, &isDir);
  if (result == ENOENT) {
    return false;
  }
  if (result != UDS_SUCCESS) {
    logWarningWithStringError(result,
                              "cannot determine if directory %s exists",
                              path);
    return false;
  }
  if (!isDir) {
    logError("expected %s to be directory", path);
    return false;
  }
  return true;
}

/**
 *  Rolls back a previous save directory to be current if needed.
 *
 *  @param fis       the file index state
 *
 *  @return UDS_SUCCESS or an error code from rename
 *
 *  @note a rollback is performed when the current directory does not exist
 *        but the previous one does
 **/
static int rollbackIfNeeded(const FileIndexState *fis)
{
  bool haveCurrent = directoryExists(fis->current);
  bool havePrevious = directoryExists(fis->previous);

  return ((!haveCurrent && havePrevious)
          ? loggingRename(fis->previous, fis->current, __func__)
          : UDS_SUCCESS);
}

/*****************************************************************************/
static int fis_loadState(IndexState *state,
                         bool       *replayPtr)
{
  FileIndexState *fis = asFileIndexState(state);

  int result = rollbackIfNeeded(fis);
  if (result != UDS_SUCCESS) {
    return result;
  }

  return genericLoadIndexState(state, replayPtr);
}

/*****************************************************************************/
static int fis_prepareSave(IndexState    *state,
                           IndexSaveType  type __attribute__((unused)))
{
  FileIndexState *fis = asFileIndexState(state);

  int result = removeDirectory(fis->deletion, DELETION_DIR);
  if ((result != UDS_SUCCESS) && (result != ENOENT)) {
    return result;
  }

  result = removeDirectory(fis->next, NEXT_DIR);
  if ((result != UDS_SUCCESS) && (result != ENOENT)) {
    return result;
  }

  return makeDirectory(fis->next, 0777, "state", __func__);
}

/*****************************************************************************/
static int fis_commitSave(IndexState *state)
{
  FileIndexState *fis = asFileIndexState(state);

  bool haveCurrent = directoryExists(fis->current);
  bool havePrevious = directoryExists(fis->previous);

  int result;
  if (haveCurrent) {
    if (havePrevious) {
      result = loggingRename(fis->previous, fis->deletion, __func__);
      if (result != UDS_SUCCESS) {
        return result;
      }
    }

    result = loggingRename(fis->current, fis->previous, __func__);
    if (result != UDS_SUCCESS) {
      return result;
    }
  }

  result = loggingRename(fis->next, fis->current, __func__);
  if (result != UDS_SUCCESS) {
    // attempt to undo above renames
    if (haveCurrent) {
      int undoResult = loggingRename(fis->previous, fis->current, __func__);
      if (undoResult != UDS_SUCCESS) {
        logWarningWithStringError(undoResult,
                                  "Failed to undo rename of %s to %s",
                                  fis->previous, fis->current);
      }

      if (havePrevious) {
        undoResult = loggingRename(fis->deletion, fis->previous, __func__);
        if (undoResult != UDS_SUCCESS) {
          logWarningWithStringError(undoResult,
                                    "Failed to undo rename of %s to %s",
                                    fis->previous, fis->deletion);
        }
      }
    }
    return result;
  }

  result = removeDirectory(fis->deletion, DELETION_DIR);
  if ((result != UDS_SUCCESS) && (result != ENOENT)) {
    return result;
  }

  return UDS_SUCCESS;
}

/*****************************************************************************/
static int fis_cleanupSave(IndexState *state)
{
  FileIndexState *fis = asFileIndexState(state);

  return removeDirectory(fis->next, NEXT_DIR);
}

/*****************************************************************************/
static int fis_writeSingleComponent(IndexState     *state,
                                    IndexComponent *component)
{
  FileIndexState *fis = asFileIndexState(state);

  int result = writeIndexComponent(component);
  if (result == UDS_SUCCESS) {
    result = makeLastComponentSaveReadable(component);
  }

  return firstError(result, removeDirectory(fis->next, NEXT_DIR));
}

/*****************************************************************************/
static int fis_discardSaves(IndexState *state, DiscardType dt)
{
  FileIndexState *fis = asFileIndexState(state);

  int result = UDS_SUCCESS;
  for (unsigned int i = 0; i < state->count; ++i) {
    int r = discardIndexComponent(state->entries[i]);
    result = firstError(result, r);
  }
  result = firstError(result, removeDirectory(fis->next, NEXT_DIR));
  result = firstError(result, removeDirectory(fis->current, CURRENT_DIR));
  if (dt == DT_DISCARD_ALL) {
    result = firstError(result, removeDirectory(fis->previous, PREVIOUS_DIR));
  } else {
    result = firstError(result, rollbackIfNeeded(fis));
  }
  return result;
}

/*****************************************************************************/

static const IndexStateOps fileIndexStateOps  = {
  .freeFunc                = fis_freeMe,
  .addComponent            = fis_addComponent,
  .loadState               = fis_loadState,
  .saveState               = genericSaveIndexState,
  .prepareSave             = fis_prepareSave,
  .commitSave              = fis_commitSave,
  .cleanupSave             = fis_cleanupSave,
  .writeCheckpoint         = genericWriteIndexStateCheckpoint,
  .writeSingleComponent    = fis_writeSingleComponent,
  .discardSaves            = fis_discardSaves,
};

static const IndexStateOps *getFileIndexStateOps(void)
{
  return &fileIndexStateOps;
}
