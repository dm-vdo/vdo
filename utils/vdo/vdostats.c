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
 * $Id: //eng/linux-vdo/src/c++/vdo/user/vdoStats.c#3 $
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "errors.h"
#include "logger.h"
#include "statistics.h"
#include "statusCodes.h"
#include "vdoStats.h"

static const char usage_string[] =
  " [--help] [--version] [options...] [device [device ...]]";

static const char help_string[] =
  "vdostats - Display configuration and statistics of VDO volumes\n"
  "\n"
  "SYNOPSIS\n"
  "  vdostats [options] [device [device ...]]\n"
  "\n"
  "DESCRIPTION\n"
  "  vdostats displays configuration and statistics information for the given\n"
  "  VDO devices. If no devices are given, it displays information about all\n"
  "  VDO devices.\n"
  "\n"
  "  The VDO devices must be running in order for configuration and\n"
  "  statistics information to be reported.\n"
  "\n"
  "OPTIONS\n"
  "    -h, --help\n"
  "       Print this help message and exit.\n"
  "\n"
  "    -a, --all\n"
  "       For backwards compatibility. Equivalent to --verbose.\n"
  "\n"
  "    --human-readable\n"
  "       Display stats in human-readable form.\n"
  "\n"
  "    --si\n"
  "       Use SI units, implies --human-readable.\n"
  "\n"
  "    -v, --verbose\n"
  "       Include statistics regarding utilization and block I/O (bios).\n"
  "\n"
  "    -V, --version\n"
  "       Print the vdostats version number and exit.\n"
  "\n";

static struct option options[] = {
  { "help",            no_argument,  NULL,  'h' },
  { "all",             no_argument,  NULL,  'a' },
  { "human-readable",  no_argument,  NULL,  'r' },
  { "si",              no_argument,  NULL,  's' },
  { "verbose",         no_argument,  NULL,  'v' },
  { "version",         no_argument,  NULL,  'V' },
  { NULL,              0,            NULL,   0  },
};

static char option_string[] = "harsvV";

enum style {
  STYLE_DF,
  STYLE_YAML,
};
enum style style = STYLE_DF;

static bool human_readable         = false;
static bool si                     = false;
static bool verbose                = false;
static bool header_printed         = false;
static int  maxDeviceNameLength = 6;

typedef struct dfStats {
  uint64_t  size;
  uint64_t  used;
  uint64_t  available;
  int       usedPercent;
  int       savingPercent;
} DFStats;

typedef struct dfFieldLengths {
  int name;
  int size;
  int used;
  int available;
  int usedPercent;
  int savingPercent;
} DFFieldLengths;

/**********************************************************************
 * Obtain the VDO device statistics.
 *
 * @param stats  The device statistics
 * 
 * @return  A DFStats structure of device statistics
 *
 **/
static DFStats getDFStats(struct vdo_statistics *stats)
{
  uint64_t size = stats->physical_blocks;
  uint64_t logicalUsed = stats->logical_blocks_used;
  uint64_t dataUsed = stats->data_blocks_used;
  uint64_t metaUsed = stats->overhead_blocks_used;
  uint64_t used =  dataUsed + metaUsed;
  uint64_t available = size - used;
  int usedPercent = (int) (100.0 * used / size + 0.5);
  int savingPercent = 0;
  if (logicalUsed > 0) {
    savingPercent = (int) (100.0 * (logicalUsed - dataUsed) / logicalUsed);
  }

  return (DFStats) {
                    .size           = size,
                    .used           = used,
                    .available      = available,
                    .usedPercent    = usedPercent,
                    .savingPercent  = savingPercent,
                   };
}

/**********************************************************************
 * Display the size in human readable format.
 *
 * @param aFieldWidth  The size field width
 * @param aSize        The size to be displayed
 *
 **/
static void printSizeAsHumanReadable(const int      aFieldWidth,
                                     const uint64_t aSize)
{
  double  size          = (double) aSize;
  int     divisor       = si ? 1000 : 1024;
  char    unitArray[5]  = { 'B', 'K', 'M', 'G', 'T' };

  unsigned int  i = 0;
  while ((size >= divisor) &&
         (i < (sizeof(unitArray) / sizeof(unitArray[0])))) {
    size /= divisor;
    i ++;
  }

  printf("%*.1f%c ", aFieldWidth - 1, size, unitArray[i]);
}

/**********************************************************************
 * Display the device statistics in DFStyle.
 *
 * @param path   The device path
 * @param stats  The device statistics
 *
 **/
static void displayDFStyle(const char *path, struct vdo_statistics *stats)
{
  const DFFieldLengths field_length = {maxDeviceNameLength, 9, 9, 9, 4, 13};
  char dfName[field_length.name + 1];
  DFStats dfStats = getDFStats(stats);

  // Extract the device name
  char *name_start = strrchr(path, '/') + 1;
  strcpy(dfName, name_start);

  // Display the device statistics
  if (!header_printed) {
    printf("%-*s %*s %*s %*s %*s %*s\n",
           field_length.name, "Device",
           field_length.size, human_readable ? "Size" : "1k-blocks",
           field_length.used, "Used",
           field_length.available, "Available",
           field_length.usedPercent, "Use%",
           field_length.savingPercent, "Space saving%");
    header_printed = true;
  }
  
  if (stats->in_recovery_mode) {
    printf("%-*s %*" PRIu64 " %*s %*s %*s %*s\n",
           field_length.name, dfName,
           field_length.size, ((dfStats.size * stats->block_size) / 1024),
           field_length.used, "N/A",
           field_length.available, "N/A",
           (field_length.usedPercent - 1), "N/A",
           (field_length.savingPercent - 1), "N/A");
    return;
  }

  if (human_readable) {
    // Convert to human readable form (e.g., G, T, P) and
    // optionally in SI units (1000 as opposed to 1024).
    printf("%-*s ", field_length.name, dfName);

    // The first argument is the field width (provided as input
    // here to ease matching any future changes with the below format
    // string).
    printSizeAsHumanReadable(field_length.size,
                             dfStats.size * stats->block_size);
    printSizeAsHumanReadable(field_length.used,
                             dfStats.used * stats->block_size);
    printSizeAsHumanReadable(field_length.available,
                             dfStats.available * stats->block_size);
  } else {
    // Convert blocks to kb for printing
    printf("%-*s %*" PRIu64 " %*" PRIu64 " %*" PRIu64 " ",
           field_length.name, dfName,
           field_length.size, dfStats.size * stats->block_size / 1024,
           field_length.used, dfStats.used * stats->block_size / 1024,
           field_length.available,
           dfStats.available * stats->block_size / 1024);
  }

  if (dfStats.savingPercent < 0) {
    printf("%*d%% %*s\n",
           (field_length.usedPercent - 1), dfStats.usedPercent,
           (field_length.savingPercent - 1), "N/A");
  } else {
    printf("%*d%% %*d%%\n",
           (field_length.usedPercent - 1), dfStats.usedPercent,
           (field_length.savingPercent - 1), dfStats.savingPercent);
  }
}

/**********************************************************************
 * Display the usage string.
 *
 * @param path  The device path
 * @param name  The dmsetup name
 *
 **/
static void usage(const char *progname, const char *usage_options_string)
{
  errx(1, "Usage: %s%s\n", progname, usage_options_string);
}

/**********************************************************************
 * Parse the arguments passed; print command usage if arguments are wrong.
 *
 * @param argc  Number of input arguments
 * @param argv  Array of input arguments
 **/
static void process_args(int argc, char *argv[])
{
  int c;

  while ((c = getopt_long(argc, argv, option_string, options, NULL)) != -1) {
    switch (c) {
    case 'h':
      printf("%s", help_string);      
      exit(0);
      break;

    case 'a':
      verbose = true;
      break;

    case 'r':
      human_readable = true;
      break;

    case 's':
      si = true;
      human_readable =  true;
      break;

    case 'v':
      verbose = true;
      break;

    case 'V':
      printf("%s version is: %s\n", argv[0], CURRENT_VERSION);
      exit(0);
      break;

    default:
      usage(argv[0], usage_string);
      break;
    };
  }
}

/**********************************************************************
 * Process the VDO stats for a single device.
 *
 * @param path  The device path
 * @param name  The dmsetup name
 *
 **/
static void process_device(const char *path, const char *name)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    warn("open failure on %s", path);
    return;
  }
  
  struct vdo_statistics stats;
  
  char dmCommand[256];
  sprintf(dmCommand, "dmsetup message %s 0 stats", name);
  FILE* fp = popen(dmCommand, "r");
  if (fp == NULL) {
    err(2, "Could not retrieve VDO device volume information");
  }
  
  char statsBuf[8192];
  if (fgets(statsBuf, sizeof(statsBuf), fp) != NULL) {
    read_vdo_stats(statsBuf, &stats);
    switch (style) {
      case STYLE_DF:
        displayDFStyle(path, &stats);
        break;

      case STYLE_YAML:
        printf("%s : \n", path);
        write_vdo_stats(&stats);
        break;

      default:
        err(2, "unknown style %d", style);
    }
  }
  pclose(fp);
}

/**********************************************************************
 * Process the VDO stats for all VDO devices.
 *
 **/
static void enumerate_devices(void)
{
  FILE *fp;
  size_t line_size = 0;
  char *dmsetup_line = NULL;
  
  fp = popen("dmsetup status --target vdo", "r");
  if (fp == NULL) {
    err(2, "Could not retrieve VDO device status information");
  }

  while ((getline(&dmsetup_line, &line_size, fp)) > 0) {
    maxDeviceNameLength =
      (((int) strlen(strtok(dmsetup_line, ":")) > maxDeviceNameLength)
       ? (int) strlen(strtok(dmsetup_line, ":"))
       : maxDeviceNameLength);
  }
  pclose(fp);
  
  fp = popen("dmsetup ls --target vdo", "r");
  if (fp == NULL) {
    err(2, "Could not retrieve VDO device volume information");
  }

  char lineBuf[256];
  while (fgets(lineBuf, sizeof(lineBuf), fp) != NULL) {
    char *tok = strtok(lineBuf, " \t\n\r");
    if (tok == NULL) {
      pclose(fp);
      err(2, "could not parse device mapper information");
    }

    char *path;
    if (asprintf(&path, "/dev/mapper/%s", tok) == -1) {
      err(1, "Failed to allocate device path buffer");
    }

    // Check whether the VDO device exists with the expected pathname.
    // This is primarily used to handle the lack of any configured
    // VDO devices.  The "dmsetup ls" command varies in its response
    // to this condition on different Linux distributions.
    struct stat buf;
    if (stat(path, &buf) == 0) {
      process_device(path, tok);
    }
    free(path);
  }

  pclose(fp);
}

/**********************************************************************/
int main(int argc, char *argv[])
{
  char err_buf[ERRBUF_SIZE];
  int result;

  result = register_vdo_status_codes();
  if (result != VDO_SUCCESS) {
    errx(1, "Could not register status codes: %s",
         string_error(result, err_buf, ERRBUF_SIZE));
  }

  process_args(argc, argv);

  if (verbose) {
    style = STYLE_YAML;
  }

  int num_devices = argc - optind;

  // Process each device
  if (num_devices == 0) {
    enumerate_devices();
  } else {
    // Set maxDeviceNameLength
    for (int i = optind; i < argc; i++) {
      char *name_start = strrchr(argv[i], '/');
      int name_length = 0;

      if (name_start == NULL) {
        name_length = (int) strlen(argv[i]);
      } else {
        name_start++;
        name_length = (int) strlen(name_start);
      }

      maxDeviceNameLength = ((name_length > maxDeviceNameLength)
                             ? name_length
                             : maxDeviceNameLength);
    }

    // Process the input devices
    for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
        continue;
      }
      // Check whether the VDO device exists. It may be input as a full path or
      // with the device name only.
      struct stat buf;
      if (stat(argv[i], &buf) == 0) {
        process_device(argv[i], basename(argv[i]));
      } else {
        char *device_path;
        if (asprintf(&device_path, "/dev/mapper/%s", argv[i]) == -1) {
          err(1, "Failed to allocate device path buffer");
        }
        if (stat(device_path, &buf) == 0) {
          process_device(device_path, argv[i]);
        }
        
        free(device_path);
      }
    }
  }
}
