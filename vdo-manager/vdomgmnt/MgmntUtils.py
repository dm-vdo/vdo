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
  MgmntUtils - miscellaneous utilities for the VDO manager

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/MgmntUtils.py#2 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from .Utils import Utils
from vdo.utils import CommandError, runCommand

class MgmntUtils(Utils):

  ######################################################################
  # Public methods
  ######################################################################
  @classmethod
  def statusHelper(cls, commandList):
    """Helper function for returning status summaries."""
    try:
      s = runCommand(commandList,
                     environment={ 'UDS_LOG_LEVEL' : 'WARNING' },
                     strip=True)
      return s.replace("\"", "")
    except CommandError:
      return _("not available")
