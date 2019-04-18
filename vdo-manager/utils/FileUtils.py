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

  FileUtils - Provides dmmgmnt file-related capabilities.

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/utils/FileUtils.py#1 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import errno
import fcntl
import gettext
import grp
import logging
import os
import stat
import tempfile
import time

from .Command import Command
from .Timeout import Timeout, TimeoutError

gettext.install("utils")

########################################################################
class FileBase(object):
  """The FileBase object; provides basic file control.

  Class attributes:
    log (logging.Logger) - logger for this class
  Attributes:
    None
  """
  log = logging.getLogger('utils.FileBase')

  ######################################################################
  # Public methods
  ######################################################################
  @property
  def path(self):
    return self.__filePath

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, filePath, *args, **kwargs):
    """
    Arguments:
      None
    Returns:
      Nothing
    """
    super(FileBase, self).__init__()
    self.__filePath = os.path.realpath(filePath)
    self.__fd = kwargs.get("fd", None)

  ######################################################################
  def __enter__(self):
    return self

  ######################################################################
  def __exit__(self, exceptionType, exceptionValue, traceback):
    # Don't suppress exceptions.
    return False

  ######################################################################
  # Protected methods
  ######################################################################
  @property
  def _fd(self):
    return self.__fd

  ######################################################################
  # pylint: disable=E0102
  # pylint: disable=E1101
  @_fd.setter
  def _fd(self, value):
    self.__fd = value

  ######################################################################
  # Private methods
  ######################################################################

########################################################################
class FileTouch(FileBase):
  """The FileTouch object; touches the file.

  Class attributes:
    log (logging.Logger) - logger for this class
  Attributes:
    None
  """
  log = logging.getLogger('utils.FileTouch')

  ######################################################################
  # Public methods
  ######################################################################

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, filePath, *args, **kwargs):
    """
    Arguments:
      None
    Returns:
      Nothing
    """
    super(FileTouch, self).__init__(filePath, *args, **kwargs)

  ######################################################################
  def __enter__(self):
    """Make certain the file exists and return ourself."""

    super(FileTouch, self).__enter__()
    if self._fd is None:
      # Make certain the file exists and that we have access to it.
      dirPath = os.path.dirname(self.path)

      # Make certain the directory exists.
      # N.B.: The names may not be sanitized for use with a shell!
      if not os.access(dirPath, os.F_OK):
        cmd = Command(["mkdir", "-p", dirPath])
        cmd.run()

      # Make certain the target exists.
      if not os.access(self.path, os.F_OK):
        self._createFile()

    return self

  ######################################################################
  # Protected methods
  ######################################################################
  def _createFile(self):
    """Creates the targe file."""
    # N.B.: The names may not be sanitized for use with a shell!
    cmd = Command(["touch", self.path])
    cmd.run()

  ######################################################################
  # Private methods
  ######################################################################

########################################################################
class FileOpen(FileTouch):
  """The FileOpen object; provides basic access to a file.

  Class attributes:
    log (logging.Logger) - logger for this class
  Attributes:
    None
  """
  log = logging.getLogger('utils.FileOpen')

  ######################################################################
  # Public methods
  ######################################################################
  @property
  def file(self):
    return self.__file

  ######################################################################
  def flush(self):
    self.file.flush()

  ######################################################################
  def read(self, numberOfBytes = -1):
    return self.file.read(numberOfBytes)

  ######################################################################
  def readline(self, numberOfBytes = -1):
    return self.file.readline(numberOfBytes)

  ######################################################################
  def readlines(self, numberOfBytesHint = None):
    # The documentation for readlines is not consistent with the other
    # read methods as to what constitutes a valid default parameter.
    # Testing shows that neither None nor -1 are acceptable so we
    # use None and specifically check for it.
    if numberOfBytesHint is None:
      return self.file.readlines()
    else:
      return self.file.readlines(numberOfBytesHint)

  ######################################################################
  def seek(self, offset, whence = os.SEEK_SET):
    self.file.seek(offset, whence)

  ######################################################################
  def truncate(self, size = None):
    # The documentation for truncate indicates that without an argument
    # it truncates to the current file position.  Testing shows that
    # neither None nor -1 are acceptable as parameters so we use None
    # and specifically check for it.
    if size is None:
      self.file.truncate()
    else:
      self.file.truncate(size)

  ######################################################################
  def write(self, string):
    self.file.write(string)

  ######################################################################
  def writelines(self, sequenceOfStrings):
    self.file.writeline(sequenceOfStrings)

  ######################################################################
  # Overridden methods
  ######################################################################
  def next(self):
    return self.file.next()

  ######################################################################
  def __enter__(self):
    """Open the file and return ourself."""
    super(FileOpen, self).__enter__()
    if self._fd is None:
      self._fd = os.open(self.path, self._osMode)
    self.__file = os.fdopen(self._fd, self.__mode)
    return self

  ######################################################################
  def __exit__(self, exceptionType, exceptionValue, traceback):
    """ Close the file."""
    self.file.close()
    return super(FileOpen, self).__exit__(exceptionType,
                                          exceptionValue,
                                          traceback)

  ######################################################################
  def __init__(self, filePath, mode = "r", *args, **kwargs):
    """
    Arguments:
      None
    Returns:
      Nothing
    """
    super(FileOpen, self).__init__(filePath, *args, **kwargs)

    osMode = None
    if (len(mode) > 1) and ("+" in mode[1:]):
      osMode = os.O_RDWR
    elif mode[0] == "r":
      osMode = os.O_RDONLY
    elif mode[0] == "w":
      osMode = os.O_WRONLY | os.O_TRUNC
    else:
      osMode = os.O_RDWR
    if mode[0] == "a":
      osMode = osMode | os.O_APPEND

    self.__file = None
    self.__mode = mode
    self.__osMode = osMode

  ######################################################################
  def __iter__(self):
    return self

  ######################################################################
  # Protected methods
  ######################################################################
  @property
  def _osMode(self):
    return self.__osMode

  ######################################################################
  # Private methods
  ######################################################################


########################################################################
class FileLock(FileOpen):
  """The FileLock object; a context manager providing interlocked access on
  a file.

  The file is created, if necessary.

  Class attributes:
    log (logging.Logger) - logger for this class
  Attributes:
    _timeout - timeout in seconds (None = no timeout)
  """
  log = logging.getLogger('utils.FileLock')

  ######################################################################
  # Public methods
  ######################################################################

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, filePath, mode, timeout=None, *args, **kwargs):
    """
    Arguments:
      filePath - (str) path to file
      mode - (str) open mode
      timeout - (int) timeout in seconds; may be None
    Returns:
      Nothing
    """
    super(FileLock, self).__init__(filePath, mode)
    self._timeout = timeout

  ######################################################################
  def __enter__(self):
    """If the open mode is read-only the file is locked shared else it is
    locked exclusively.
    """
    super(FileLock, self).__enter__()
    if self._osMode == os.O_RDONLY:
      flockMode = fcntl.LOCK_SH
      lockModeString = "shared"
    else:
      flockMode = fcntl.LOCK_EX
      lockModeString = "exclusive"

    if self._timeout is not None:
      self.log.debug("attempting to lock {f} in {s}s mode {m}"
                     .format(f=self.path,
                             s=self._timeout,
                             m=lockModeString))
      with Timeout(self._timeout, _(
          "Could not lock {f} in {s} seconds").format(f=self.path,
                                                      s=self._timeout)):
        fcntl.flock(self.file, flockMode)
    else:
      self.log.debug("attempting to lock {f} mode {m}"
                     .format(f=self.path,
                             m=lockModeString))
      fcntl.flock(self.file, flockMode)
    return self

  ######################################################################
  def __exit__(self, exceptionType, exceptionValue, traceback):
    """ Unlocks and closes the file."""
    fcntl.flock(self.file, fcntl.LOCK_UN)
    if exceptionType is not TimeoutError:
      self.log.debug("released lock {f}".format(f=self.path))

    return super(FileLock, self).__exit__(exceptionType,
                                          exceptionValue,
                                          traceback)

  ######################################################################
  # Protected methods
  ######################################################################

  ######################################################################
  # Private methods
  ######################################################################

