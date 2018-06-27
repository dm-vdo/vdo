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
 * $Id: //eng/vdo-releases/aluminum/src/c++/vdo/user/vdoDMEventd.c#1 $
 */

#include <dlfcn.h>
#include <err.h>
#include <getopt.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <libdevmapper.h>
#include <libdevmapper-event.h>

#include "logger.h"
#include "types.h"

static const char usageString[] =
  " [--help] [options...] vdo";

static const char helpString[] =
  "vdodmeventd - register/unregister VDO device with dmeventd\n"
  "\n"
  "SYNOPSIS\n"
  "  vdodmeventd [options] <vdo device name>\n"
  "\n"
  "DESCRIPTION\n"
  "  vdodmeventd handles registration of VDO devices with dmeventd\n"
  "\n"
  "OPTIONS\n"
  "    --help\n"
  "       Print this help message and exit.\n"
  "\n"
  "    --register\n"
  "       Register a VDO device with dmeventd.\n"
  "\n"
  "    --unregister\n"
  "       Unregister a VDO device with dmeventd.\n"
  "\n"
  "    --version\n"
  "       Show the version of vdodmeventd.\n"
  "\n";

// N.B. the option array must be in sync with the option string.
static struct option options[] = {
  { "help",                     no_argument,       NULL, 'h' },
  { "register",                 no_argument,       NULL, 'r' },
  { "unregister",               no_argument,       NULL, 'u' },
  { "version",                  no_argument,       NULL, 'V' },
  { NULL,                       0,                 NULL,  0  },
};
static char optionString[] = "hruV";

static void usage(const char *progname, const char *usageOptionsString)
{
  errx(1, "Usage: %s%s\n", progname, usageOptionsString);
}

/**
 * Creates an event handler to be used for various dmeventd calls.
 * 
 * @param devName Name of VDO device
 * @param dso     Name of VDO plugin
 *
 * @returns pointer to event handler on success, otherwise NULL
 */
static struct dm_event_handler *createEventHandler(const char *devName, 
						   const char *dso)
{
  int result = 0;

  struct dm_event_handler *dmevh = dm_event_handler_create();
  if (dmevh == NULL) {
    logError("Failure to create event hander");
    return NULL;
  }

  result = (dso != NULL) && dm_event_handler_set_dso(dmevh, dso);
  if (result != 0) {
    logError("Failure to set plugin name for %s", devName);
    dm_event_handler_destroy(dmevh);
    return NULL;
  }
                    
  result = dm_event_handler_set_dev_name(dmevh, devName);
  if (result != 0) {
    logError("Failure to set VDO device name for %s", devName);
    dm_event_handler_destroy(dmevh);
    return NULL;
  }
            
  // Note: no error return code
  dm_event_handler_set_event_mask(dmevh, 
				  DM_EVENT_ALL_ERRORS | DM_EVENT_TIMEOUT);
  return dmevh;
}

/**
 * Gets the current registration state for the VDO device.
 *
 * @param devName  Name of vdo device
 * @param dso      Name of vdo plugin to use, null will ignore setting
 * 
 * @returns -1 if error, 0 if device not registered, non zero event
 *          mask of registered events if device registered    
 **/
static int getRegistrationState(char *devName, char *dso, int *pending)
{
  int result = 0;
  enum dm_event_mask evmask = 0;

  *pending = 0;

  struct dm_event_handler *dmevh = createEventHandler(devName, dso);
  if (dmevh == NULL) {
    return -1;
  }

  result = dm_event_get_registered_device(dmevh, 0);
  if (result != 0) {
    dm_event_handler_destroy(dmevh);
    return 0;
  }

  evmask = dm_event_handler_get_event_mask(dmevh);
  if (evmask & DM_EVENT_REGISTRATION_PENDING) {
    evmask &= ~DM_EVENT_REGISTRATION_PENDING;
    *pending = 1;
  }

  dm_event_handler_destroy(dmevh);
  return evmask;
}

enum registerType { EVENTS_REGISTER, EVENTS_UNREGISTER };

/**
 * Used to register all needed events.
 *
 * @param type    Whether to register or unregister
 * @param devName Name of vdo device
 * @param dso     Name of the plugin to use
 *
 * @returns 0 is success, otherwise error
 */
static int processEvents(enum registerType type, char *devName, char *dso)
{
  int result = 0;

  struct dm_event_handler *dmevh = createEventHandler(devName, dso);
  if (dmevh == NULL) {
    logError("Failed to create event handler for %s", devName);
    return 1;
  }

  // Note: 1 is valid. 0 is error here.
  result = (type == EVENTS_REGISTER) ? dm_event_register_handler(dmevh) 
                                     : dm_event_unregister_handler(dmevh);
  if (result == 0) {
    logError("Failure to process events for %s", devName);
  }

  dm_event_handler_destroy(dmevh);

  return !result;
}

/**
 * Registers a vdo device with dmeventd monitoring
 *
 * @param devName Name of vdo device to register
 * 
 * @returns 0 is success, otherwise error
 */
static int registerVDO(char * devName) {
  int result = 0;
  int pending = 0; 

  result = getRegistrationState(devName, PLUGIN_NAME, &pending);
  if (result < 0) {
    logError("Failed to get registration info for VDO device %s", 
	     devName);
    return 1;
  }

  if (result > 0) {
    logError("VDO device %s %s", devName,
	     pending ? "has a registration event pending" 
	             : "is already being monitored");
    return 1;
  }

  result = processEvents(EVENTS_REGISTER, devName, PLUGIN_NAME);
  if (result != 0) {
    logError("Unable to register events for VDO device %s", devName);
    return result;
  } 

  logInfo("VDO device %s is now registered with dmeventd "
	  "for monitoring", devName);

  return 0;
}

/**
 * Unregisters a vdo device with dmeventd monitoring
 *
 * @param devName Name of vdo device to unregister
 * 
 * @returns 0 is success, otherwise error
 */
static int unregisterVDO(char * devName) {
  int result = 0;
  int pending = 0;

  result = getRegistrationState(devName, NULL, &pending);
  if (result < 0) {
    logError("Failed to get registration info for VDO device %s", 
	     devName);
    return 1;
  }

  if ((result == 0) || (pending == 1)) {
    logError("VDO device %s %s", devName,
	     pending ? "cannot be unregistered until completed" 
	             : "is not currently being monitored");
    return 1;
  }

  result = processEvents(EVENTS_UNREGISTER, devName, NULL);
  if (result != 0) {
    logError("Unable to unregister dmeventd events for VDO "
	     "device %s", devName);
    return result;
  }

  logInfo("VDO device %s is now unregistered from dmeventd", 
	  devName);

  return 0;  
}

/**
 * Validate that dynamic shared libray exists
 *
 * @returns 0 if success, otherwise error
 */
static int validatePlugin(void)
{
  void *dl = dlopen(PLUGIN_NAME, RTLD_NOW);
  if (!dl) {
    logError("The dynamic shared library %s could not "
	     "be loaded: %s", PLUGIN_NAME, dlerror());
    return 1; /* Failure. */
  }
        
  dlclose(dl);
  return 0; /* Valid. */
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  int c;
  bool doRegister = false;
  bool doUnregister = false;

  while ((c = getopt_long(argc, argv, optionString, options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", helpString);
      exit(0);
      break;

   case 'r':
      doRegister = true;
      break;

    case 'u':
      doUnregister = true;
      break;

    case 'V':
      fprintf(stdout, "vdodmeventd version is: %s\n", CURRENT_VERSION);
      exit(0);
      break;

    default:
      usage(argv[0], usageString);
      break;
    };
  }

  if (optind != (argc - 1)) {
    usage(argv[0], usageString);
  }

  char *devName = argv[optind];

  openLogger();

  if (doRegister && doUnregister) {
    errx(1, "Only one of -r and -u can be specified");
  }

  if (!doRegister && !doUnregister) {
    errx(1, "Neither -r nor -u was specified");
  }
  
  if (validatePlugin()) {
    logError("Failed to load the dmeventd plugin");
    return 1;
  }

  if (doRegister) {
    return registerVDO(devName);
  }
 
  return unregisterVDO(devName);
}
