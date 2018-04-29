#!/usr/bin/perl

##
# Copyright (c) 2018 Red Hat, Inc.
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
# nagios_check_albserver.pl <vdoName>
# 
# This script parses the output of "vdo status" for a given VDO volume,
# determines if UDS deduplication is enabled, and if so, returns
# a Nagios-compliance status code and single-line output for the 
# deduplication status.
# 
# The "vdo" program must be in the path used by "sudo".  The user executing
# this program should be granted permission via sudoers (with the "NOPASSWD:"
# option) to execute the "vdo" program.
#
# $Id: //eng/vdo-releases/magnesium-rhel7.5/src/tools/nagios/nagios_check_albserver.pl#1 $
#
## 

use strict;
use warnings FATAL => qw(all);
use List::MoreUtils qw(first_index true);

# Constants for the Nagios service status return values.
use constant {
  NAGIOS_SERVICE_OK       => 0,
  NAGIOS_SERVICE_WARNING  => 1,
  NAGIOS_SERVICE_CRITICAL => 2,
  NAGIOS_SERVICE_UNKNOWN  => 3,
};

if (scalar(@ARGV) != 1) {
  print("Usage: nagios_check_albserver.pl <vdoName>\n");
  exit(NAGIOS_SERVICE_UNKNOWN);
}

my $vdoName = $ARGV[0];
my @vdoStatusOutput = `sudo vdo status --name=$vdoName 2>&1`;

my $badPath = true {/vdo: command not found/} @vdoStatusOutput;
if ($badPath > 0) {
  print("The vdo command is not in the current path.\n");
  exit(NAGIOS_SERVICE_UNKNOWN);
}

my $notFound = true {/VDO volume.*not found/} @vdoStatusOutput;
if ($notFound > 0) {
  print("VDO volume $vdoName not found.\n");
  exit(NAGIOS_SERVICE_UNKNOWN);
}

my $udsDedupeConfigIndex = first_index {/Deduplication:/}
                           @vdoStatusOutput;

my $udsServerStatusIndex = first_index {/Index status:/}
                           @vdoStatusOutput;

my $udsDedupeConfig = $vdoStatusOutput[$udsDedupeConfigIndex];
chomp($udsDedupeConfig);

# Default message
my $udsServerStatus = "Index is not running.";

# If the index is running, $udsServerStatusIndex will not be -1. 
if ($udsServerStatusIndex != -1) {
  # The index of the "Server status" data is one line below the title string.
  $udsServerStatus = "$vdoStatusOutput[$udsServerStatusIndex]";
  chomp($udsServerStatus);
}

if ($udsServerStatus =~ "online\$") {
  print("Deduplication is online.\n");
  exit(NAGIOS_SERVICE_OK);
}

if ($udsDedupeConfig =~ "disabled") {
  print("Deduplication is disabled.\n");
  exit(NAGIOS_SERVICE_OK);
}

print("$udsServerStatus\n");
exit(NAGIOS_SERVICE_CRITICAL);
