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
  Utils - miscellaneous utilities for the VDO manager

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/Utils.py#2 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from vdo.utils import CommandError, runCommand
import distutils.spawn
import errno
import os
import signal
import time

class Utils(object):
  """Utils contains miscellaneous utilities."""

  KiB = 1024
  MiB = 1024 * KiB
  GiB = 1024 * MiB
  TiB = 1024 * GiB
  PiB = 1024 * TiB

  ######################################################################
  # Public methods
  ######################################################################
  @staticmethod
  def abspathPath(path):
    """Takes a path or a colon-separated list of paths and makes
    each one an absolute path. Paths that don't exist are left alone."""
    return os.pathsep.join([os.path.abspath(p) for p in path.split(os.pathsep)])

  ######################################################################
  @staticmethod
  def appendToPath(path):
    """Appends a directory or directories to the current PATH.

    Arguments:
      path (str): A directory or colon-separated list of directories.
    """
    os.environ['PATH'] += os.pathsep + path

  ######################################################################
  @staticmethod
  def isPowerOfTwo(i):
    """Returns True iff its argument is a power of two."""
    return (i != 0) and ((i & (i - 1)) == 0)

  ######################################################################
  @classmethod
  def killProcess(cls, pid):
    """Kills a process and waits for it to die.

    Arguments:
      pid:      process id to kill
    Throws:
      Any unexpected OS error
    """
    try:
      os.kill(pid, signal.SIGTERM)
      while True:
        time.sleep(1)
        os.kill(pid, 0)
    except OSError as e:
      if e.errno != errno.ESRCH:
        raise e

  ######################################################################
  @staticmethod
  def maxNum(a, b):
    """Returns the maximum of two numbers."""
    if a > b:
      return a
    return b

  ######################################################################
  @staticmethod
  def which(cmd):
    """Finds the full path to a command.

    Arguments:
      cmd (str): The command to search for.
    Returns:
      The full path as a string, or None if the command is not found.
    """
    return distutils.spawn.find_executable(cmd)

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    pass
