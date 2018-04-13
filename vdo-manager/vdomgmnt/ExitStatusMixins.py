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
  ExitStatusMixins - Mixins for exceptions to associate distinguishing exit
                      status values

  These mixins are defined based on the types of error that can occur.
  Each mixin is assigned a specific exit status value to disambiguate the
  error type via exit status of user-facing utilities.

  The values start at 3 to provide distinction from common failure exit
  statuses (1 - something went wrong, 2 - argument error) that may be produced
  by user-facing utilities.

  In accord with mixin best practices these mixins are to be specified
  *before* an exception's superclass exception in the exception's definition.
  This allows for a subsystem base exception to provide a subsystem-wide
  exit status and for subsystem specific exceptions to specialize the
  exit status as appropriate.

  To avoid subclass proliferation simply to provide specific exit statuses
  one can chose to create a subsystem base exception using any of the
  ExitStatus hierachy classes (though, generally, one should probably only
  use ExitStatus itself) and provide a specific exit status for an exception
  at instantiation by passing any of the ExitStatus hierarchy classes as the
  'exitStatus' instantiation parameter.

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/ExitStatusMixins.py#1 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

class ExitStatus(object):
  """Base class for all ExistStatusMixins.
  Provides a default exit status value to be interpreted solely as an
  error occurred.
  """
  _exitStatusMixinValue = 3

  ######################################################################
  # Public methods
  ######################################################################
  @property
  def exitStatus(self):
    if self._exitStatus is not None:
      return self._exitStatus._exitStatusMixinValue
    return self._exitStatusMixinValue;

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, exitStatus = None, *args, **kwargs):
    super(ExitStatus, self).__init__(*args, **kwargs)
    self._exitStatus = exitStatus
    if ((self._exitStatus is not None)
        and (not issubclass(self._exitStatus, ExitStatus))):
      raise TypeError(
        "{0} is not an ExitStatus mixin".format(
                                              type(self._exitStatus).__name__))

########################################################################
class DeveloperExitStatus(ExitStatus):
  """Used to represent an error condition due to a developer oversight.
  """
  _exitStatusMixinValue = 4

########################################################################
class StateExitStatus(ExitStatus):
  """Used to represent an error condition due to the state of some entity.
  """
  _exitStatusMixinValue = 5

########################################################################
class SystemExitStatus(ExitStatus):
  """Used to represent an error condition due to a failure on the part of
  the operating system, hardware, etc.
  """
  _exitStatusMixinValue = 6

########################################################################
class UserExitStatus(ExitStatus):
  """Used to represent an error condition due to the user; e.g., bad parameter,
  insufficient permissions, etc.
  """
  _exitStatusMixinValue = 7
