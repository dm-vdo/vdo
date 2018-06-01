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
  CommandLock - simple process locking

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/CommandLock.py#2 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from vdo.utils import Command, FileLock

class CommandLockError(Exception):
  """Exception raised to indicate an error acquiring a CommandLock."""
  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, message, *args):
    super(CommandLockError, self).__init__()
    self._message = message.format(*args)

  ######################################################################
  def __str__(self):
    return self._message

########################################################################
class CommandLock(FileLock):
  """Simple process locking.
  """

  ######################################################################
  # Overridden methods
  ######################################################################
  def __enter__(self):
    try:
      super(CommandLock, self).__enter__()
    except:
      raise CommandLockError("Could not lock file {0}", self.path)
    return self

  ######################################################################
  def __init__(self, filePath, readonly=True):
    super(CommandLock, self).__init__(filePath,
                                      "r" if readonly else "r+",
                                      timeout = 20)

  ######################################################################
  def __repr__(self):
    lst = [str(self), "["]
    lst.append(','.join('='.join([key, str(getattr(self, key))])
                        for key in self.__dict__))
    lst.append("]")
    return "".join(lst)

  ######################################################################
  def __str__(self):
    return "{0}({1})".format(type(self).__name__, self.path)

  ######################################################################
  def _createFile(self):
    super(CommandLock, self)._createFile()
    # For now, we default to permitting shared locks from non-root
    # processes. The sysadmin may change this; we won't change the
    # permissions once the file has been created.
    #
    # N.B.: The names may not be sanitized for use with a shell!
    cmd = Command(["chmod", "644", self.path])
    cmd.run()

  ######################################################################
  # Protected methods
  ######################################################################
