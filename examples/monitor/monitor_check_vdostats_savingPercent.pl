#!/usr/bin/perl

##
# Copyright Red Hat
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
# monitor_check_vdostats_savingPercent.pl [--warning <warn_pct>|-w <warn_pct>]
#                                         [--critical <crit_pct>|-c <crit_pct>]
#                                         <deviceName>
#
# This script parses the output of "vdostats --verbose" for a given VDO
# volume, processes the "used percent" value, and returns a status code,
# and a single-line output with status information.
#
# Options:
#
# -c <crit_pct>: critical threshold equal to or greater than
#                <warn_pct> percent.
#
# -w <warn_pct>: warning threshold equal to or less than
#                <crit_pct> percent.
#
# The "vdostats" program must be in the path used by "sudo".
#
##

use strict;
use warnings FATAL => qw(all);
use Getopt::Long;

# Constants for the service status return values.
use constant {
  MONITOR_SERVICE_OK       => 0,
  MONITOR_SERVICE_WARNING  => 1,
  MONITOR_SERVICE_CRITICAL => 2,
  MONITOR_SERVICE_UNKNOWN  => 3,
};

my $inputWarnThreshold = -1;
my $inputCritThreshold = -1;

GetOptions("critical=i" => \$inputCritThreshold,
           "warning=i"  => \$inputWarnThreshold);

# Default warning and critical thresholds for "logical used percent".
my $warnThreshold = 50;
my $critThreshold = 5;

if ($inputWarnThreshold >= 0 && $inputWarnThreshold <= 100) {
  $warnThreshold = $inputWarnThreshold;
}

if ($inputCritThreshold >= 0 && $inputCritThreshold <= 100) {
  $critThreshold = $inputCritThreshold;
}

# A hash to hold the statistics names and values gathered from input.
my %stats = ();

# Vital statistics for general VDO health.  This array contains only the
# names of the desired statistics to store in the %stats hash.
my @statNames = (
  'operating mode',
  'data blocks used',
  'overhead blocks used',
  'logical blocks used',
  'physical blocks',
  'logical blocks',
  'used percent',
  'saving percent',
  '1k-blocks available',
);

#############################################################################
# Get the statistics output for the given VDO device name, and filter the
# desired stats values.
##
sub getStats {
  if (!$ARGV[0]) {
    return;
  }
  my $deviceName = $ARGV[0];
  my @verboseStatsOutput = `sudo vdostats $deviceName --verbose`;
  foreach my $statLabel (@statNames) {
    foreach my $inpline (@verboseStatsOutput) {
      if ($inpline =~ $statLabel) {
        $inpline =~ /.*: (.*)$/;
        my $statValue = $1;
        $stats{$statLabel} = $statValue;
      }
    }
  }
}

#############################################################################
# main
##
if (scalar(@ARGV) != 1) {
  print("Usage: monitor_check_vdostats_savingPercent.pl\n");
  print("                [--warning |-w VALUE]\n");
  print("                [--critical|-c VALUE]\n");
  print("                <deviceName>\n");
  exit(MONITOR_SERVICE_UNKNOWN);
}

getStats();

# If the stats table is empty, nothing was found; return unknown status.
# Otherwise, print the stats.
if (!%stats) {
  printf("Unable to load vdostats verbose output.\n");
  exit(MONITOR_SERVICE_UNKNOWN);
}

printf("saving percent: %s%%\n", $stats{"saving percent"});

# Process the critical and warning thresholds.
# If critThreshold is less than warnThreshold, the only used percentage
# return codes will be "OK" or "CRITICAL".

# An empty VDO volume has a saving percent of "N/A" when 0 logical blocks
# are used. This is interpreted as an "OK" status.
if ($stats{"saving percent"} =~ "N/A") {
  exit(MONITOR_SERVICE_OK);
}

if ($stats{"saving percent"} <= $warnThreshold
    && $stats{"saving percent"} > $critThreshold) {
  exit(MONITOR_SERVICE_WARNING);
}

if ($stats{"saving percent"} <= $critThreshold) {
  exit(MONITOR_SERVICE_CRITICAL);
}

if ($stats{"saving percent"} > $warnThreshold) {
  exit(MONITOR_SERVICE_OK);
}

# Default exit condition.
exit(MONITOR_SERVICE_UNKNOWN);
