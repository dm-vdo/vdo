#
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

"""
  MgmntLogger - VDO manager logging

  $Id: //eng/vdo-releases/magnesium-rhel7.5/src/python/vdo/vdomgmnt/MgmntLogger.py#1 $

"""
from utils import Logger
import os

class MgmntLogger(Logger):

  ######################################################################
  # Overridden methods
  ######################################################################
  @classmethod
  def configure(cls, name, logfile = None, debug = False):
    debug = debug or (int(os.environ.get('VDO_DEBUG', '0')) > 0)
    super(MgmntLogger, cls).configure(name, logfile, debug)
