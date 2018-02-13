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
 * $Id: //eng/uds-releases/flanders/src/uds/histogram.h#2 $
 */

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <stdio.h>

#include "typeDefs.h"

typedef struct histogram Histogram;

/**
 * Allocate and initialize a histogram that uses linearly sized buckets.
 *
 * @param initLabel The label for the sampled data.  This label is used
 *                  when we plot the data.
 * @param size      The number of buckets.  There are buckets for every
 *                  value from 0 up to size (but not including) size.
 *                  There is an extra bucket for larger samples.
 *
 * @return the histogram
 **/
Histogram *makeLinearHistogram(const char *initLabel, int size);

/**
 * Allocate and initialize a histogram that uses logarithmically sized
 * buckets.
 *
 * @param initLabel The label for the sampled data.  This label is used
 *                  when we plot the data.
 * @param logSize   The number of buckets.  There are buckets for a range
 *                  of sizes up to 10^logSize, and an extra bucket for
 *                  larger samples.
 *
 * @return the histogram
 **/
Histogram *makeLogarithmicHistogram(const char *initLabel, int logSize);

/**
 * Enter a sample into a histogram
 *
 * @param h       The histogram
 * @param sample  The sample
 **/
void enterHistogramSample(Histogram *h, uint64_t sample);

/**
 * Print the histogram data.
 *
 * @param f  The stdio file to write to.
 * @param h  The histogram.
 **/
void printHistogram(FILE *f, const Histogram *h);

/**
 * Log the histogram data.
 *
 * @param priority  The logging priority level to use
 * @param h         The histogram
 **/
void logHistogram(int priority, const Histogram *h);

/**
 * Log the histogram data as a bar graph.
 *
 * @param priority  The logging priority level to use
 * @param h         The histogram
 **/
void logHistogramBarGraph(int priority, const Histogram *h);

/**
 * Plot a single histogram.
 *
 * @param name The base name of the histogram.  We append ".gnuplot" to
 *             make the gnuplot script, and ".gif" for the graph.  If the
 *             value is NULL, we use the short name of the running program.
 * @param h    The histogram.
 **/
void plotHistogram(const char *name, Histogram *h);

/**
 * Plot one or more histograms on a single graph.
 *
 * @param name The base name of the histogram.  We append ".gnuplot" to
 *             make the gnuplot script, and ".gif" for the graph.  If the
 *             value is NULL, we use the short name of the running program.
 *             This is followed be a NULL terminated list of histograms.
 **/
void plotHistograms(const char *name, ...);

/**
 * Free a histogram and null out the reference to it.
 *
 * @param hp  The reference to the histogram.
 **/
void freeHistogram(Histogram **hp);

#endif /* HISTOGRAM_H */
