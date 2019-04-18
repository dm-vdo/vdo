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
  SizeString - LVM-style size strings

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/SizeString.py#1 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
import locale
from . import Constants

class SizeString(object):
  """Represents the size of an object such as a disk partition.

  Conversions are provided to and from suffixed size strings as used
  by LVM commands like lvcreate(8). These strings consist of a
  (possibly floating-point) number followed by an optional unit
  suffix: B (bytes), S (512-byte sectors), and KMGTPE for kilobytes
  through exabytes, respectively. Suffixes are not case-sensitive; the
  default unit is Megabytes. Currently, we reject negative sizes.

  Unlike some (but not all) LVM commands we do not interpret the upper-case
  version of a suffix as a power of ten.

  Attributes:
    _originalString (str): the original string we were constructed
      with, mainly used for debugging
    _bytes (int): the value of this object in bytes

  """

  ######################################################################
  # Public methods
  ######################################################################
  def __add__(self, rhs):
    retval = SizeString("")
    retval._bytes = self._bytes + int(rhs)
    return retval

  ######################################################################
  def __bool__(self):
    return self._bytes != 0

  ######################################################################
  def __eq__(self, rhs):
    if not isinstance(rhs, SizeString):
      result = NotImplemented
    else:
      result = (self._bytes == rhs.toBytes())
    return result
              
  ######################################################################
  def __ne__(self, rhs):
    if not isinstance(rhs, SizeString):
      result = NotImplemented
    else:
      result = (self._bytes != rhs.toBytes())
    return result
              
  ######################################################################
  def __lt__(self, rhs):
    if not isinstance(rhs, SizeString):
      result = NotImplemented
    else:
      result = (self._bytes < rhs.toBytes())
    return result
              
  ######################################################################
  def __le__(self, rhs):
    if not isinstance(rhs, SizeString):
      result = NotImplemented
    else:
      result = (self._bytes <= rhs.toBytes())
    return result
              
  ######################################################################
  def __gt__(self, rhs):
    if not isinstance(rhs, SizeString):
      result = NotImplemented
    else:
      result = (self._bytes > rhs.toBytes())
    return result
              
  ######################################################################
  def __ge__(self, rhs):
    if not isinstance(rhs, SizeString):
      result = NotImplemented
    else:
      result = (self._bytes >= rhs.toBytes())
    return result
              
  ######################################################################
  def __iadd__(self, rhs):
    self._bytes += int(rhs)
    return self

  ######################################################################
  def __int__(self):
    return self._bytes

  ######################################################################
  def __nonzero__(self):
    return self.__bool__()

  ######################################################################
  def asLvmText(self):
    """Returns this object as a size string without a decimal point."""
    suffix = Constants.lvmDefaultSuffix
    size = self._bytes
    if size > 0:
      for click in Constants.lvmSiSuffixes[::-1]:
        divisor = Constants.lvmSiSuffixSizeMap[click]
        if size % divisor == 0:
          size = size // divisor
          suffix = click
          break
      else:
          suffix = Constants.lvmByteSuffix

    return str(size) + suffix.upper()

  ######################################################################
  def roundToBlock(self):
    """Rounds this object down to a multiple of the block size."""
    self._bytes = self.toBlocks() * Constants.VDO_BLOCK_SIZE
    return self

  ######################################################################
  def toBlocks(self):
    """Returns this object as a count of 4K blocks, rounding down."""
    return (self._bytes // Constants.VDO_BLOCK_SIZE)

  ######################################################################
  def toBytes(self):
    """Returns the count of bytes represented by this object."""
    return self._bytes

  ######################################################################
  def toSectors(self):
    """Returns this object as a count of 512-byte sectors, rounding up."""
    bytesPerSector = Constants.lvmSuffixSizeMap[Constants.lvmSectorSuffix]
    return (self._bytes + (bytesPerSector - 1)) // bytesPerSector

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, sz):
    self._originalString = sz
    if sz:
      try:
        suffix = sz[-1:].lower()
        if suffix in Constants.lvmSuffixes:
          nbytes = self._atof(sz[:-1])
        else:
          nbytes = self._atof(sz)
          suffix = Constants.lvmDefaultSuffix
      except ValueError:
        raise ValueError(_("invalid size string \"{size}\"").format(size=sz))
      nbytes *= float(Constants.lvmSuffixSizeMap[suffix])
      self._bytes = int(nbytes)
      if self._bytes < 0:
        raise ValueError(_("invalid size string \"{size}\"").format(size=sz))
    else:
      self._bytes = 0

  ######################################################################
  def __repr__(self):
    return "{0} ({1}{2})".format(self._originalString,
                                 self._bytes,
                                 Constants.lvmByteSuffix.upper())

  ######################################################################
  def __str__(self):
    return self.asLvmText()

  ######################################################################
  # Protected methods
  ######################################################################
  @staticmethod
  def _atof(s):
    """Tries to convert a float using the current LC_NUMERIC settings.
    If something goes wrong, tries float().
    """
    try:
      return locale.atof(s)
    except Exception:
      return float(s)
