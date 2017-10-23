#
# Copyright (c) 2017 Red Hat, Inc.
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
  Defaults - manage Albireo/VDO defaults

  $Id: //eng/vdo-releases/magnesium/src/python/vdo/vdomgmnt/Defaults.py#1 $

"""
from . import Constants, MgmntLogger, MgmntUtils, SizeString
import optparse
import os
import re

class ArgumentError(Exception):
  """Exception raised to indicate an error with an argument."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, msg):
    super(ArgumentError, self).__init__()
    self._msg = msg

  ######################################################################
  def __str__(self):
    return self._msg

########################################################################
class Defaults(object):
  """Defaults manages default values for arguments."""

  NOTSET = -1
  ackThreads = 1
  ackThreadsMax = 100
  ackThreadsMin = 0
  activate = Constants.enabled
  bioRotationInterval = 64
  bioRotationIntervalMax = 1024
  bioRotationIntervalMin = 1
  bioThreadReadCacheOverheadMB = 1.12 # per read cache MB overhead in MB
  bioThreads = 4
  bioThreadsMax = 100
  bioThreadsMin = 1
  blockMapCacheSize = SizeString("128M")
  blockMapCacheSizeMaxPlusOne = SizeString("16T")
  blockMapCacheSizeMin = SizeString("128M")
  blockMapPeriod = 16380
  blockMapPeriodMax = 16380
  blockMapPeriodMin = 1
  cfreq = 0
  confFile = os.getenv('VDO_CONF_DIR', '/etc') + '/vdoconf.yml'
  compression = Constants.enabled
  deduplication = Constants.enabled
  cpuThreads = 2
  cpuThreadsMax = 100
  cpuThreadsMin = 1
  emulate512 = Constants.disabled
  hashZoneThreads = 1
  hashZoneThreadsMax = 100
  hashZoneThreadsMin = 0
  indexMem = 0.25
  log = MgmntLogger.getLogger(MgmntLogger.myname + '.Defaults')
  logicalThreads = 1
  logicalThreadsMax = 100
  logicalThreadsMin = 0
  mdRaid5Mode = 'on'
  physicalThreads = 1
  physicalThreadsMax = 100
  physicalThreadsMin = 0
  readCache = Constants.disabled
  readCacheSize = SizeString("0")
  slabSize = SizeString("2G")
  slabSizeMax = SizeString("32G")
  slabSizeMin = SizeString("128M")
  sparseIndex = Constants.disabled
  udsParallelFactor = 0
  vdoPhysicalBlockSize = 4096
  vdoLogLevel = 'info'
  writePolicy = 'sync'

  ######################################################################
  # Public methods
  ######################################################################
  @staticmethod
  def checkAbspath(unused_option, opt, value):
    """Checks that an option is an absolute pathname.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The pathname as a string.
    Raises:
      OptionValueError
    """
    if os.path.isabs(value):
      return value
    raise optparse.OptionValueError(
      _("option {0!s}: must be an absolute pathname").format(opt))

  ######################################################################
  @staticmethod
  def checkBlkDev(unused_option, opt, value):
    """Checks that an option is a valid name for the backing store.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The backing store name as a string.
    Raises:
      OptionValueError
    """
    return Defaults.checkAbspath(unused_option, opt, value)

  ######################################################################
  @staticmethod
  def checkIndexmem(unused_option, opt, value):
    """Checks that an option is a legitimate index memory setting.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The memory setting as a string.
    Raises:
      OptionValueError
    """
    try:
      if value == '0.25' or value == '0.5' or value == '0.75':
        return value
      int(value)
      return value
    except ValueError:
      pass
    raise optparse.OptionValueError(
      _("option {0!s}: must be an index memory value").format(opt))

  ######################################################################
  @staticmethod
  def checkPageCachesz(unused_option, opt, value):
    """Checks that an option is an acceptable value for the page cache size.
    Page cache sizes will be rounded up to a multiple of the page size, and
    must be at least 128M and less than 16T.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      OptionValueError
    """
    ss = Defaults.checkSiSize(unused_option, opt, value)
    if (not (Defaults.blockMapCacheSizeMin
             <= ss < Defaults.blockMapCacheSizeMaxPlusOne)):
      raise optparse.OptionValueError(
        _("option {0!s}: must be at least {1} and less than {2}")
          .format(opt, Defaults.blockMapCacheSizeMin,
                  Defaults.blockMapCacheSizeMaxPlusOne))
    return ss

  ######################################################################
  @staticmethod
  def checkPagesz(unused_option, opt, value):
    """Checks that an option is an acceptable value for a page size.
    Page sizes must be a power of 2, and are normally interpreted as a
    byte count. The suffixes 'K' and 'M' may be used to specify
    kilobytes or megabytes, respectively.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to an integer byte count.
    Raises:
      OptionValueError
    """
    try:
      multipliers = {Constants.lvmKiloSuffix.lower() : Constants.KB,
                     Constants.lvmMegaSuffix.lower() : Constants.MB}
      regex = r"^(\d+)([{0}{1}{2}{3}])?$".format(
                Constants.lvmKiloSuffix.lower(),
                Constants.lvmKiloSuffix.upper(),
                Constants.lvmMegaSuffix.lower(),
                Constants.lvmMegaSuffix.upper())

      m = re.match(regex, value)
      if (m):
        nbytes = int(m.group(1))
        if m.group(2):
          nbytes *= multipliers[m.group(2).lower()]
        if MgmntUtils.isPowerOfTwo(nbytes):
          return nbytes
    except ValueError:
      pass
    raise optparse.OptionValueError(
      _("option {0!s}: must be a power of 2, {1}/{2} suffix optional").format(
        opt, Constants.lvmKiloSuffix.upper(), Constants.lvmMegaSuffix.upper()))

  ######################################################################
  @staticmethod
  def checkPow2(unused_option, opt, value):
    """Checks that an option is an integer power of two.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to an integer.
    Raises:
      OptionValueError
    """
    try:
      n = int(value)
      if MgmntUtils.isPowerOfTwo(n):
        return n
    except ValueError:
      pass
    raise optparse.OptionValueError(
      _("option {0!s}: must be an integer power of 2").format(opt))

  ######################################################################
  @staticmethod
  def checkSiSize(unused_option, opt, value):
    """Checks that an option is an SI unit size string.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      OptionValueError
    """
    if (value[-1].isdigit()
        or (value[-1].isalpha()
            and (value[-1].lower() in Constants.lvmSiSuffixes))):
      try:
        ss = SizeString(value)
        return ss
      except ValueError:
        pass
    raise optparse.OptionValueError(
      _("option {0!s}: must be an SI-style size string").format(opt))

  ######################################################################
  @staticmethod
  def checkSize(unused_option, opt, value):
    """Checks that an option is an LVM-style size string.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      OptionValueError
    """
    try:
      ss = SizeString(value)
      return ss
    except ValueError:
      pass
    raise optparse.OptionValueError(
      _("option {0!s}: must be an LVM-style size string").format(opt))

  ######################################################################
  @staticmethod
  def checkSlabSize(unused_option, opt, value):
    """Checks that an option is a valid slab size.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The value converted to a SizeString.
    Raises:
      OptionValueError
    """
    try:
      ss = SizeString(value)
      size = ss.toBytes()
      if size == 0:
        # We must be using the default.
        return ss
      if ((not MgmntUtils.isPowerOfTwo(size))
          or (not (Defaults.slabSizeMin <= ss <= Defaults.slabSizeMax))):
        raise optparse.OptionValueError(
          _("option {0!s}: must be a power of two between {1} and {2}")
            .format(opt, Defaults.slabSizeMin, Defaults.slabSizeMax))
      return ss
    except ValueError:
      pass
    raise optparse.OptionValueError(
      _("option {0!s}: must be an LVM-style size string").format(opt))

  ######################################################################
  @staticmethod
  def checkVDOName(unused_option, opt, value):
    """Checks that an option is a valid VDO device name.

    The "dmsetup create" command will accept a lot of characters that
    could be problematic for udev or for running shell commands
    without careful quoting. For now, we permit only alphanumerics and
    a few other characters.

    Arguments:
      opt (str): Name of the option being checked.
      value (str): Value provided as an argument to the option.
    Returns:
      The device name as a string.
    Raises:
      OptionValueError

    """
    # See "whitelisted" characters in
    # https://sourceware.org/git/?p=lvm2.git;a=blob;f=libdm/libdm-common.c;h=e983b039276671cae991f9b8b81328760aacac4a;hb=HEAD
    #
    # N.B.: (1) Moved "-" last so we can put in it brackets in a
    # regexp. (2) Removed "=" so "-name=foo" ("-n ame=foo") gets an
    # error.
    allowedChars = "A-Za-z0-9#+.:@_-"
    if re.match("^[" + allowedChars + "]*$", value) is None:
      raise optparse.OptionValueError(
        _("option {0}: VDO device names may only contain characters"
          " in '{1}': bad value '{2}'")
        .format(opt, allowedChars, value))
    if value.startswith("-"):
      raise optparse.OptionValueError(
        _("option {0}: VDO device names may not start with '-':"
          " bad value '{1}'")
        .format(opt, value))
    return value

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    pass
