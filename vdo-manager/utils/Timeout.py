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

  Timeout - context manager that implements a timeout.

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/utils/Timeout.py#1 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
import signal

########################################################################
class TimeoutError(Exception):
  """Exception raised when a block times out."""
  def __init__(self, msg = None, *args, **kwargs):
    super(TimeoutError, self).__init__(*args, **kwargs)
    if msg is None:
      msg = "timeout"
    self._msg = msg
  def __str__(self):
    return self._msg

########################################################################
class Timeout(object):
  """Context manager for running a block of commands under a timeout.
  If the block times out, a TimeoutError is raised.

  Arguments:
    seconds (int) - timeout in seconds
    msg (str) - message to supply to TimeoutError
  """
  ######################################################################
  # Public methods
  ######################################################################

  ######################################################################
  # Overridden methods
  ######################################################################
  def __enter__(self):
    # Establish the alarm handler and set the alarm to go off.
    self.__oldHandler = signal.signal(signal.SIGALRM,
                                      lambda _signum, _frame : self._timeout())

    signal.alarm(self.__seconds)
    return self

  ######################################################################
  def __exit__(self, exceptionType, exceptionValue, traceback):
    # Turn off the alarm and re-establish the previous alarm handler.
    signal.alarm(0)
    signal.signal(signal.SIGALRM, self.__oldHandler)

    # Don't suppress exceptions.
    return False

  ######################################################################
  def __init__(self, seconds, msg = None):
    self.__seconds = seconds
    self.__msg = msg
    self.__oldHandler = None

  ######################################################################
  # Protected methods
  ######################################################################
  def _timeout(self):
    """ Method invoked if the alarm goes off.
    """
    raise TimeoutError(self.__msg)

  ######################################################################
  # Private methods
  ######################################################################
