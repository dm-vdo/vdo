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
  KernelModuleService - manages the kvdo kernel module

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/KernelModuleService.py#2 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from . import Defaults, Service
from vdo.utils import Command, CommandError, runCommand
import yaml


class KernelModuleService(Service):
  """KernelModuleService manages a kernel module on the local node."""
  ######################################################################
  # Public methods
  ######################################################################
  def running(self, wait=True):
    """Returns True if the module is loaded and DM target is available."""
    retries = 20 if wait else 1
    try:
      runCommand(["lsmod", "|", "grep", "-q", "'" + self._name + "'"],
                 shell=True, retries=retries)
      runCommand(["dmsetup", "targets", "|", "grep", "-q",
                  Defaults.vdoTargetName], shell=True, retries=retries)
      return True
    except CommandError:
      return False

  ######################################################################
  def start(self):
    """Loads the module if necessary."""
    runCommand(['modprobe', self._name])

  ######################################################################
  def status(self):
    """Returns a dictionary representing the status of this object.
    """
    return { _("Name") : self._name,
             _("Loaded") : self.running(False),
             _("Version information") : yaml.safe_load(self.version()) }

  ######################################################################
  def stop(self):
    """Removes the module."""
    runCommand(['modprobe', '-r', self._name])

  ######################################################################
  def version(self):
    """Returns the module version as a string."""
    s = self._name + " "
    for line in runCommand(['modinfo', self._name], noThrow=True).splitlines():
      if line.find('version') == 0:
        s += line
    return s

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, name):
    super(KernelModuleService, self).__init__(name)

  ######################################################################
  # Protected methods
  ######################################################################
  def _reprAttribute(self, key):
    """KernelModuleService does not exclude any of its __dict__ contents
       from its __repr__ result.
    """
    return True
