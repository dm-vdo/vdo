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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoReadOnly.c#1 $
 */

#include <err.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#include "errors.h"
#include "fileUtils.h"
#include "logger.h"

#include "constants.h"
#include "physicalLayer.h"
#include "statusCodes.h"

#include "fileLayer.h"
#include "vdoConfig.h"
#include "vdoVolumeUtils.h"

/**********************************************************************/
int main(int argc, char *argv[])
{
  static char errBuf[ERRBUF_SIZE];

  int result = registerStatusCodes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         stringError(result, errBuf, ERRBUF_SIZE));
  }

  if ((argc != 2) || (argv[1][0] == '-')) {
    fprintf(stderr, "Usage:  vdoReadOnly device\n");
    exit(1);
  }

  char *filename = argv[1];

  openLogger();

  PhysicalLayer *layer;
  result = makeFileLayer(filename, 0, &layer);
  if (result != VDO_SUCCESS) {
    errx(result, "makeFileLayer failed on '%s'", filename);
  }

  result = setVDOReadOnlyMode(layer);
  if (result != VDO_SUCCESS) {
    char buf[ERRBUF_SIZE];
    errx(result, "setting read-only mode failed on '%s': %s",
         filename, stringError(result, buf, sizeof(buf)));
  }

  // Close and sync the uderlying file.
  layer->destroy(&layer);
}
