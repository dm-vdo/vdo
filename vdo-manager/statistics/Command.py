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
  Command -- a command which is implemented as an ioctl

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/statistics/Command.py#1 $
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import fcntl

class Command(object):
  """
  Command represents a command which may be sent to a VDO via an ioctl.
  Commands have no return data. For ioctls which fetch statistics, use
  StatStruct.
  """
  def __init__(self, ioctl):
    """
    :param ioctl: The numeric value of the ioctl for this command.
    """
    self.ioctl = ioctl

  def act(self, device):
    """
    Send the command to a device.

    :param device: The name of the device on which to act.
    """
    with open(device, 'r') as fd:
      result = fcntl.ioctl(fd, self.ioctl)
      if result:
        raise Exception("ioctl failed with result {0}".format(result))
