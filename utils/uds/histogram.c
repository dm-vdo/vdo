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
 * $Id: //eng/uds-releases/flanders-rhel7.5/src/uds/histogram.c#1 $
 */

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "common.h"
#include "histogram.h"
#include "logger.h"
#include "memoryAlloc.h"

/*
 * Support histogramming of the Albireo code.
 *
 * This is not a complete and general histogram package.  It follows the XP
 * practise of implementing the "customer" requirements, and no more.  We
 * can support other requirements after we know what they are.
 *
 * All samples are uint64_t values.  All of the histograms we have done so
 * far have used the difference between two nowUsec() values to generate a
 * time in microseconds.
 */

struct histogram {
  unsigned int *counters;  // Counter for each bucket
  uint64_t *topValue;      // The top value for each bucket
  const char *label;       // Histogram label
  int numBuckets;          // The number of buckets
  bool logFlag;            // True if the y scale should be logarithmic
};

/**
 * Allocate the buckets for a histogram
 *
 * @param h  The histogram, with the numBuckets field already filled in.
 *
 * @return UDS_SUCCESS or an error code
 **/
static int allocateBuckets(Histogram *h)
{
  int result = ALLOCATE(h->numBuckets + 1, unsigned int,
                        "histogram counters", &h->counters);
  if (result != UDS_SUCCESS) {
    return result;
  }
  return ALLOCATE(h->numBuckets, uint64_t, "histogram tops",
                  &h->topValue);
}

/***********************************************************************/
Histogram *makeLinearHistogram(const char *initLabel, int size)
{
  Histogram *h;
  if (ALLOCATE(1, Histogram, "histogram", &h) != UDS_SUCCESS) {
    return NULL;
  }
  h->label      = initLabel;
  h->logFlag    = false;
  h->numBuckets = size;

  if (allocateBuckets(h) != UDS_SUCCESS) {
    freeHistogram(&h);
    return NULL;
  }
  for (int i = 0; i < h->numBuckets; i++) {
    h->topValue[i] = i;
  }
  return h;
}

/***********************************************************************/
Histogram *makeLogarithmicHistogram(const char *initLabel, int logSize)
{
  Histogram *h;
  if (ALLOCATE(1, Histogram, "histogram", &h) != UDS_SUCCESS) {
    return NULL;
  }
  h->label      = initLabel;
  h->logFlag    = true;
  h->numBuckets = 10 * logSize + 1;

  if (allocateBuckets(h) != UDS_SUCCESS) {
    freeHistogram(&h);
    return NULL;
  }
  for (int i = 0; i < h->numBuckets; i++) {
    if (i <= 10) {
      h->topValue[i] = i;
    } else {
      h->topValue[i] = floor(exp10((double) i / 10.0));
    }
  }
  return h;
}

/***********************************************************************/
void enterHistogramSample(Histogram *h, uint64_t sample)
{
  int lo = 0;
  int hi = h->numBuckets;

  while (lo < hi) {
    int middle = (lo + hi) / 2;
    if (sample <= h->topValue[middle]) {
      hi = middle;
    } else {
      lo = middle + 1;
    }
  }
  ++h->counters[lo];
}

typedef void (*HistogramDumper)(void *callbackArgument,
                                const char *format,
                                ...)
  __attribute__((format(printf, 2, 0)));

/***********************************************************************/
static unsigned int divideRoundingToNearest(uint64_t number, uint64_t divisor)
{
  number += divisor/2;
  return number / divisor;
}

/***********************************************************************/
static void dumpHistogram(HistogramDumper  dumper,
                          void            *callbackArgument,
                          const Histogram *h,
                          bool             bars)
{
  int max = h->numBuckets;
  while ((max >= 0) && (h->counters[max] == 0)) {
    max--;
  }
  // If max reaches -1, we'll fall through to reporting the total of zero.

  enum { BAR_SIZE = 50 };
  char bar[BAR_SIZE + 2];
  bar[0] = ' ';
  memset(bar + 1, '=', BAR_SIZE);
  bar[BAR_SIZE + 1] = '\0';

  uint64_t total = 0;
  for (int i = 0; i <= max; i++) {
    total += h->counters[i];
  }

  for (int i = 0; i <= max; i++) {
    unsigned int barLength;
    if (bars) {
      // +1 for the space at the beginning
      barLength = (divideRoundingToNearest(h->counters[i] * BAR_SIZE, total)
                   + 1);
    } else {
      // 0 means skip the space and the bar
      barLength = 0;
    }
    if (h->logFlag) {
      if (i == h->numBuckets) {
        dumper(callbackArgument, "%-16s : %12d%.*s", "Bigger",
               h->counters[i], barLength, bar);
      } else {
        dumper(callbackArgument, "%6d - %7d : %12d%.*s",
               (i == 0) ? 0 : (int) h->topValue[i - 1] + 1,
               (int) h->topValue[i], h->counters[i], barLength, bar);
      }
    } else {
      if (i == h->numBuckets) {
        dumper(callbackArgument, "%6s : %12d%.*s", "Bigger",
               h->counters[i], barLength, bar);
      } else {
        dumper(callbackArgument, "%6d : %12d%.*s",
               i, h->counters[i], barLength, bar);
      }
    }
  }
}

/***********************************************************************/
__attribute__((format(printf, 2, 0)))
static void doFprintf(void *callbackArgument, const char *format, ...)
{
  FILE *f = callbackArgument;
  va_list args;
  va_start(args, format);
  vfprintf(f, format, args);
  va_end(args);
  putc('\n', f);
}

/***********************************************************************/
void printHistogram(FILE *f, const Histogram *h)
{
  dumpHistogram(doFprintf, f, h, false);
}

/***********************************************************************/
__attribute__((format(printf, 2, 0)))
static void doLog(void *callbackArgument, const char *format, ...)
{
  int priority = *(int *)callbackArgument;
  va_list args;
  va_start(args, format);
  vLogMessage(priority, format, args);
  va_end(args);
}

/***********************************************************************/
void logHistogram(int priority, const Histogram *h)
{
  if (h != NULL) {
    logMessage(priority, "%s", h->label);
    dumpHistogram(doLog, &priority, h, false);
  }
}

/***********************************************************************/
void logHistogramBarGraph(int priority, const Histogram *h)
{
  if (h != NULL) {
    logMessage(priority, "%s", h->label);
    dumpHistogram(doLog, &priority, h, true);
  }
}

/***********************************************************************/
void plotHistogram(const char *name, Histogram *h)
{
  plotHistograms(name, h, NULL);
}

/***********************************************************************/
void plotHistograms(const char *name, ...)
{
  int nameLen;
  if (name == NULL) {
    name = program_invocation_short_name;
    char *nameExt = strrchr(program_invocation_short_name, '.');
    if (nameExt == NULL) {
      nameLen = strlen(name);
    } else {
      nameLen = nameExt - name;
    }
  } else {
    nameLen = strlen(name);
  }
  char *gpPath;
  int result = allocSprintf(__func__, &gpPath, "%.*s.gnuplot", nameLen, name);
  if (result != UDS_SUCCESS) {
    return;
  }
  FILE *gpfile = fopen(gpPath, "w");
  fprintf(gpfile, "#!/usr/bin/gnuplot\n");
  fprintf(gpfile, "set logscale y\n");
  fprintf(gpfile, "set xlabel \"Microseconds\"\n");
  fprintf(gpfile, "set ylabel \"Count\"\n");
  fprintf(gpfile, "set term gif size 1200,800\n");
  fprintf(gpfile, "set output \"%.*s.gif\"\n", nameLen, name);
  va_list ap;
  const Histogram *h;
  va_start(ap, name);
  while ((h = va_arg(ap, const Histogram *)) != NULL) {
    if (h->logFlag) {
      fprintf(gpfile, "set logscale x\n");
      break;
    }
  }
  va_end(ap);
  va_start(ap, name);
  for (int hIndex = 0; (h = va_arg(ap, const Histogram *)) != NULL; hIndex++) {
    const char *prefix = hIndex == 0 ? "plot" : ",";
    fprintf(gpfile, "%s \"-\" with lines title \"%s\"", prefix, h->label);
  }
  va_end(ap);
  fprintf(gpfile, "\n");
  va_start(ap, name);
  for (int hIndex = 0; (h = va_arg(ap, const Histogram *)) != NULL; hIndex++) {
    for (int i = 0; i < h->numBuckets; i++) {
      fprintf(gpfile, "%u %u\n", (unsigned int) h->topValue[i],
              h->counters[i]);
    }
    fprintf(gpfile, "e\n");
  }
  va_end(ap);
  fclose(gpfile);
  chmod(gpPath, 0777);
  char *command;
  result = allocSprintf(__func__, &command, "./%s", gpPath);
  FREE(gpPath);
  if (result == UDS_SUCCESS) {
    int result = system(command);
    if (result != 0) {
      logWarning("Could not run command %s: error %i", command, result);
    }
    FREE(command);
  }
}

/***********************************************************************/
void freeHistogram(Histogram **hp)
{
  if (*hp != NULL) {
    Histogram *h = *hp;
    FREE(h->counters);
    FREE(h->topValue);
    FREE(h);
    *hp = NULL;
  }
}
