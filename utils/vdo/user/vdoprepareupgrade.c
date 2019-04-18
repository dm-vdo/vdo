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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoPrepareUpgrade.c#1 $
 */

#include <err.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fileUtils.h"
#include "logger.h"
#include "syscalls.h"

#include "constants.h"
#include "physicalLayer.h"
#include "upgrade.h"

#include "fileLayer.h"

/*
 *  vdoPrepareUpgrade confirms that the version of the target VDO
 *  is still Oxygen, and that its state is CLEAN.  It then sets
 *  the state to FORCE_REBUILD, converts the SlabConfig to
 *  Fluorine-compatible values, and clears the Recovery Journal
 *  partition.  When it saves this, the version changes to Fluorine's
 *  version, so when it's brought up, VDO will rebuild the device to
 *  be compatible with torn-write protection.
 */

/**********************************************************************/
int main(int argc, char *argv[])
{
  char *filename;
  filename = argv[argc - 1];

  static char errBuf[ERRBUF_SIZE];

  int result = registerStatusCodes();
  if (result != UDS_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         stringError(result, errBuf, ERRBUF_SIZE));
  }

  openLogger();

  PhysicalLayer *layer;
  result = makeFileLayer(filename, 0, &layer);
  if (result != VDO_SUCCESS) {
    errx(result, "makeFileLayer failed on '%s'", filename);
  }

  result = upgradePriorVDO(layer);
  layer->destroy(&layer);
  if (result != VDO_SUCCESS) {
    errx(1, "Could not upgrade VDO at '%s': %s",
         filename, stringError(result, errBuf, ERRBUF_SIZE));
  }

  warnx("Successfully saved upgraded VDO");
}
