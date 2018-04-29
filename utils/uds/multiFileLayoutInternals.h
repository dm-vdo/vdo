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
 * $Id: //eng/uds-releases/flanders-rhel7.5/src/uds/multiFileLayoutInternals.h#1 $
 */

#ifndef MULTI_FILE_LAYOUT_INTERNALS_H
#define MULTI_FILE_LAYOUT_INTERNALS_H

#include "multiFileLayout.h"

#include "compiler.h"
#include "indexLayout.h"
#include "permassert.h"
#include "util/pathBuffer.h"

typedef struct multiFileLayout MultiFileLayout;

struct multiFileLayout {
  IndexLayout   common;
  char         *indexDir;       // this should be const but C is stupid
  char         *sealFilename;   //   ditto
  char         *configFilename; //   ditto
  PathBuffer    volumePath;
  size_t        volumeCommon;
  void        (*cleanupFunc)(MultiFileLayout *);
  int         (*fiddleRegion)(IORegion *);
};

/*****************************************************************************/
static INLINE MultiFileLayout *asMultiFileLayout(IndexLayout *layout)
{
  return container_of(layout, MultiFileLayout, common);
}

/*****************************************************************************/
int initializeMultiFileLayout(MultiFileLayout *mfl, const char *path,
                              void (*cleanupFunc)(MultiFileLayout *),
                              int (*fiddleRegion)(IORegion *))
  __attribute__((warn_unused_result));

/*****************************************************************************/
void destroyMultiFileLayout(MultiFileLayout *mfl);

#endif // MULTI_FILE_LAYOUT_INTERNALS_H
