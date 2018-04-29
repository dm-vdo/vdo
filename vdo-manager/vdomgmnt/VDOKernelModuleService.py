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
  VDOKernelModuleService - manages the kvdo kernel module

  $Id: //eng/vdo-releases/magnesium-rhel7.5/src/python/vdo/vdomgmnt/VDOKernelModuleService.py#1 $

"""
from . import Defaults
from .KernelModuleService import KernelModuleService
from utils import runCommand
import string

class VDOKernelModuleService(KernelModuleService):
  """KernelModuleService manages the kvdo kernel module on the local node."""

  ######################################################################
  # Public methods
  ######################################################################
  def setLogLevel(self, level):
    """Sets the module log level."""
    if level != Defaults.vdoLogLevel:
      runCommand(string.split("echo" + level + " > /sys/" + self._name
                              + "/log_level"), shell=True, noThrow=True)

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(VDOKernelModuleService, self).__init__('kvdo')

  ######################################################################
  # Protected methods
  ######################################################################
