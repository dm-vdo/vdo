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
  VDOArgumentParser - argument parser for vdo command input

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/VDOArgumentParser.py#9 $
"""
# "Too many lines in module"
#pylint: disable=C0302

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

# Deliberately don't include unicode_literals. If we do, the argument
# names become unicode strings instead of bytes, and repr(argument) is
# u'argument' instead of the pretty 'argument'. argparse uses repr not
# str to print allowable arguments in case of error, so we want to avoid
# printing spare u's (see VDO-4170).

import argparse
import gettext

from . import Constants
from .Defaults import ArgumentError, Defaults

gettext.install('vdo')

########################################################################
# "Line too long"
#pylint: disable=C0301
class VDOArgumentParser(argparse.ArgumentParser):
  """Argument parser for the vdo command.

  Attributes:
  lvmOptionalSuffix (str): describes usage of lvm suffixes
  lvmOptionalSiSuffix (str): describes usage of lvm SI suffixes
  """

  lvmOptionalSuffix = _("""Using a value with a {options} or {last} suffix
is optional""").format(options = ", ".join([Constants.lvmSuffixTextMap[suffix]
                                            for suffix
                                              in Constants.lvmSuffixes[:-1]]),
                       last = Constants.lvmSuffixTextMap[
                                                  Constants.lvmSuffixes[-1]])

  lvmOptionalSiSuffix = _("""Using a value with a {options} or {last}
suffix is optional""").format(options
                              = ", ".join([Constants.lvmSiSuffixTextMap[suffix]
                                           for suffix
                                             in Constants.lvmSiSuffixes[:-1]]),
                              last
                                = Constants.lvmSiSuffixTextMap[
                                                  Constants.lvmSiSuffixes[-1]])

  ####################################################################
  class CommandArgumentParser(argparse.ArgumentParser):
    """Argument parser type to use for commands.

    Provides command-identifying "unrecognized arguments" error rather
    than having the unrecognized arguments bubble up to the root parser and
    result in a non-specific "unrecognized arguments" error.
    """
    ##################################################################
    def __init__(self, *args, **kwargs):
      super(VDOArgumentParser.CommandArgumentParser, self).__init__(*args,
                                                                    **kwargs)
      self._redirected = False

    ##################################################################
    def parse_known_args(self, args = None, namespace = None):
      """Redirects the command's argument parsing through parse_arg()
      which will, if there are unknown arguments, result in a cmmand-specific
      "unrecognized arguments" message.
      """
      result = None
      if not self._redirected:
        self._redirected = True
        namespace = self.parse_args(args, namespace)
        self._redirected = False
        result = (namespace, [])
      else:
        result = super(VDOArgumentParser.CommandArgumentParser,
                       self).parse_known_args(args, namespace)

      return result

  ####################################################################
  # Public methods
  ####################################################################

  ####################################################################
  # Overridden methods
  ####################################################################
  def parse_args(self, args = None, namespace = None):
    namespace = super(VDOArgumentParser, self).parse_args(args, namespace)
    self.__postParseChecks(namespace)
    return namespace

  ####################################################################
  def __init__(self, *args, **kwargs):
    if kwargs.get("description") is None:
      kwargs["description"] = _("""
        Manage kernel VDO devices and related configuration information. For
        help on individual commands specify the command followed by --help.
        Unless otherwise noted all commands must be run with root privileges.
                                """)
    super(VDOArgumentParser, self).__init__(*args, **kwargs)

    subparserAdder = self.add_subparsers(
                    title = "management commands",
                    help = "description",
                    dest = "command",
                    metavar = "command",
                    parser_class = VDOArgumentParser.CommandArgumentParser)

    self.__commonOptions = self._commonOptionsParser()
    self.__namingOptions = self._namingOptionsParser()

    # activate command.
    highLevelHelp = _("""
      Activates one or more VDO volumes. Activated volumes can be started
      using the 'start' command.
                      """)
    description = _("""
      {0} This command must be run with root privileges.
                    """).format(highLevelHelp)
    self._activateCommandParser = subparserAdder.add_parser(
      "activate",
      parents = [self.__namingOptions, self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # changeWritePolicy command.
    highLevelHelp = _("""
      Modifies the write policy of one or all running VDO volumes.
                      """)
    description = _("""
      {0} This command must be run with root privileges.
                    """).format(highLevelHelp)
    self._changeWritePolicyCommandParser = subparserAdder.add_parser(
      "changeWritePolicy",
      parents = [self.__namingOptions,
                 self._writePolicyOptionParser(required = True),
                 self.__commonOptions],
      help = highLevelHelp,
      description = description)
    # create command.
    highLevelHelp = _("""
      Creates a VDO volume and its associated index and makes it available.
                      """)
    description = _("""
      {0} If --activate={1} is specified the VDO volume is created but not made
      available. If the specified device is already in use by a VDO volume (as
      determined from the configuration file) the create will always be 
      rejected, even if --force is specified. If the device is not so in use
      but is formatted as a VDO volume or contains an existing file system
      the create will be rejected unless --force is given.
                    """).format(highLevelHelp, Constants.disabled)
    self._createCommandParser = subparserAdder.add_parser(
      "create",
       parents = [self._nameOptionParser(),
                  self._deviceOptionParser(),
                  self._activateOptionParser(),
                  self._blockMapCacheSizeOptionParser(),
                  self._blockMapPeriodOptionParser(),
                  self._compressionOptionParser(),
                  self._deduplicationOptionParser(),
                  self._emulate512OptionParser(),
                  self._forceOptionParser(),
                  self._indexMemOptionParser(),
                  self._maxDiscardSizeOptionParser(),
                  self._sparseIndexOptionParser(),
                  self._uuidOptionParser(),
                  self._vdoAckThreadsOptionParser(),
                  self._vdoBioRotationIntervalOptionParser(),
                  self._vdoBioThreadsOptionParser(),
                  self._vdoCpuThreadsOptionParser(),
                  self._vdoHashZoneThreadsOptionParser(),
                  self._vdoLogicalSizeOptionParser(),
                  self._vdoLogicalThreadsOptionParser(),
                  self._vdoLogLevelOptionParser(),
                  self._vdoPhysicalThreadsOptionParser(),
                  self._vdoSlabSizeOptionParser(),
                  self._writePolicyOptionParser(),
                  self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # deactivate command.
    highLevelHelp = _("""
      Deactivates one or more VDO volumes. Deactivated volumes cannot be
      started by the 'start' command. Deactivating a currently running
      volume does not stop it.
                      """)
    description = _("""
      {0} Once stopped a deactivated VDO volume must be activated before
      it can be started again. This command must be run with root
      privileges.
                    """).format(highLevelHelp)
    self._deactivateCommandParser = subparserAdder.add_parser(
      "deactivate",
      parents = [self.__namingOptions, self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # disableCompression command.
    highLevelHelp = _("""
      Disables compression on one or more VDO volumes. If the VDO volume is
      running, takes effect immediately.
                      """)
    description = _("""
      {0} If the VDO volume is not running compression will be disabled the
      next time the VDO volume is started. This command must be run with root
      privileges.
                    """).format(highLevelHelp)
    self._disableCompressionCommandParser = subparserAdder.add_parser(
      "disableCompression",
      parents = [self.__namingOptions, self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # disableDeduplication command.
    highLevelHelp = _("""
      Disables deduplication on one more VDO volumes. If the VDO volume is
      running, takes effect immediately.
                      """)
    description = _("""
      {0} If the VDO volume is not running deduplication will be disabled the
      next time the VDO volume is started. This command must be run with root
      privileges.
                    """).format(highLevelHelp)
    self._disableDeduplicationCommandParser = subparserAdder.add_parser(
      "disableDeduplication",
      parents = [self.__namingOptions, self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # enableCompression command.
    highLevelHelp = _("""
      Enables compression on one or more VDO volumes. If the VDO volume is
      running, takes effect immediately.
                      """)
    description = _("""
      {0} If the VDO volume is not running compression will be enabled the
      next time the VDO volume is started. This command must be run with root
      privileges.
                    """).format(highLevelHelp)
    self._enableCompressionCommandParser = subparserAdder.add_parser(
      "enableCompression",
      parents = [self.__namingOptions, self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # enableDeduplication command.
    highLevelHelp = _("""
      Enables deduplication on one or more VDO volumes. If the VDO volume
      is running, takes effect immediately.
                      """)
    description = _("""
      {0} If the VDO volume is not running deduplication will be enabled the
      next time the VDO volume is started. This command must be run with root
      privileges.
                    """).format(highLevelHelp)
    self._enableDeduplicationCommandParser = subparserAdder.add_parser(
      "enableDeduplication",
      parents = [self.__namingOptions, self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # growLogical command.
    highLevelHelp = _("""
      Grows the logical size of a VDO volume. The volume must exist and
      must be running.
                      """)
    description = _("""
      {0} This command must be run with root privileges.
                    """).format(highLevelHelp)
    self._growLogicalCommandParser = subparserAdder.add_parser(
      "growLogical",
      parents = [self._nameOptionParser(),
                 self._vdoLogicalSizeOptionParser(required = True),
                 self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # growPhysical command.
    highLevelHelp = _("""
      Grows the physical size of a VDO volume. The volume must exist and
      must be running.
                      """)
    description = _("""
      {0} This command must be run with root privileges.
                    """).format(highLevelHelp)
    self._growPhysicalCommandParser = subparserAdder.add_parser(
      "growPhysical",
      parents = [self._nameOptionParser(), self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # import command.
    highLevelHelp = _("""
      Creates a VDO volume from an existing VDO formatted storage device by
      importing it into VDO manager for use.
                      """)
    description = _("""
      {0} If --activate={1} is specified the VDO volume is created but not made
      available. This command must be run with root privileges.
                    """).format(highLevelHelp, Constants.disabled)

    self._importCommandParser = subparserAdder.add_parser(
      "import",
       parents = [self._nameOptionParser(),
                  self._deviceOptionParser(),
                  self._activateOptionParser(),
                  self._blockMapCacheSizeOptionParser(),
                  self._blockMapPeriodOptionParser(),
                  self._compressionOptionParser(),
                  self._deduplicationOptionParser(),
                  self._emulate512OptionParser(),
                  self._maxDiscardSizeOptionParser(),
                  self._uuidOptionParser(),
                  self._vdoAckThreadsOptionParser(),
                  self._vdoBioRotationIntervalOptionParser(),
                  self._vdoBioThreadsOptionParser(),
                  self._vdoCpuThreadsOptionParser(),
                  self._vdoHashZoneThreadsOptionParser(),
                  self._vdoLogicalThreadsOptionParser(),
                  self._vdoLogLevelOptionParser(),
                  self._vdoPhysicalThreadsOptionParser(),
                  self._writePolicyOptionParser(),
                  self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # list command.
    highLevelHelp = _("""
      Displays a list of started VDO volumes. If --all is specified it
      displays both started and non-started volumes.
                      """)
    description = _("""
      {0} This command must be run with root privileges.
                    """).format(highLevelHelp)
    self._listCommandParser = subparserAdder.add_parser(
      "list",
      parents = [self._allOptionParser(), self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # modify command.
    highLevelHelp = _("""
      Modifies configuration parameters of one or all VDO volumes. Changes
      take effect the next time the VDO device is started; already-running
      devices are not affected.
                      """)
    description = _("""
      {0} This command must be run with root privileges.
                    """).format(highLevelHelp)
    self._changeableModifyOptions = ["blockMapCacheSize",
                                     "blockMapPeriod",
                                     "maxDiscardSize",
                                     "uuid",
                                     "vdoAckThreads",
                                     "vdoBioRotationInterval",
                                     "vdoBioThreads",
                                     "vdoCpuThreads",
                                     "vdoHashZoneThreads",
                                     "vdoLogicalThreads",
                                     "vdoPhysicalThreads"]
    self._modifyCommandParser = subparserAdder.add_parser(
      "modify",
      parents = [self.__namingOptions,
                 self._blockMapCacheSizeOptionParser(noDefault = True),
                 self._blockMapPeriodOptionParser(noDefault = True),
                 self._maxDiscardSizeOptionParser(noDefault = True),
                 self._uuidOptionParser(noDefault = True),
                 self._vdoAckThreadsOptionParser(noDefault = True),
                 self._vdoBioRotationIntervalOptionParser(noDefault = True),
                 self._vdoBioThreadsOptionParser(noDefault = True),
                 self._vdoCpuThreadsOptionParser(noDefault = True),
                 self._vdoHashZoneThreadsOptionParser(noDefault = True),
                 self._vdoLogicalThreadsOptionParser(noDefault = True),
                 self._vdoPhysicalThreadsOptionParser(noDefault = True),
                 self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # printConfigFile command.
    highLevelHelp = _("""
      Prints the configuration file to stdout. This command does not require
      root privileges.
                      """)
    description = _("""
      {0}
                    """).format(highLevelHelp)
    self._printConfigFileCommandParser = subparserAdder.add_parser(
      "printConfigFile",
      parents = [self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # remove command.
    commandText = _("""
                    """)
    highLevelHelp = _("""
      Removes one or more stopped VDO volumes and associated indexes.
                      """)
    description = _("""
      {0} This command must be run with root privileges.
                    """).format(highLevelHelp)
    self._removeCommandParser = subparserAdder.add_parser(
      "remove",
      parents = [self.__namingOptions,
                 self._forceOptionParser(),
                 self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # start command.
    highLevelHelp = _("""
      Starts one or more stopped, activated VDO volumes and associated
      services.
                      """)
    description = _("""
      {0} This command must be run with root privileges.
                    """).format(highLevelHelp)
    self._startCommandParser = subparserAdder.add_parser(
      "start",
      parents = [self.__namingOptions,
                 self._forceRebuildOptionParser(),
                 self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # status command.
    highLevelHelp = _("""
      Reports VDO system and volume status in YAML format. This command does
      not require root privileges though information will be incomplete if
      run without.
                      """)
    description = _("""
      {0}
                    """).format(highLevelHelp)
    self._statusCommandParser = subparserAdder.add_parser(
      "status",
      parents = [self._namingOptionsParser(required = False),
                 self.__commonOptions],
      help = highLevelHelp,
      description = description)

    # stop command.
    commandText = _("""
                    """)
    highLevelHelp = _("""
      Stops one or more running VDO volumes and associated services.
                      """)
    description = _("""
      {0} This command must be run with root privileges.
                    """).format(highLevelHelp)
    self._stopCommandParser = subparserAdder.add_parser(
      "stop",
      parents = [self.__namingOptions,
                 self._forceOptionParser(),
                 self.__commonOptions],
      help = highLevelHelp,
      description = description)

  ####################################################################
  # Protected methods
  ####################################################################
  def _activateOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--activate",
                        choices =  Constants.enableChoices,
                        help = _("""
      Indicates if the VDO volume should, in addition to being created, be
      activated and started. The default is {activate}.
                                 """)
      .format(activate = Defaults.activate))

    return parser

  ####################################################################
  def _allOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    self.__parserAddAllOption(parser)
    return parser

  ####################################################################
  def _blockMapCacheSizeOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault
                      else
                   _("The default is {0}.").format(Defaults.blockMapCacheSize))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--blockMapCacheSize",
                        type = self.__optionCheck(Defaults.checkPageCachesz),
                        metavar = "<megabytes>",
                        help = _("""
      Specifies the amount of memory allocated for caching block map pages;
      the value must be a multiple of {pageSize}. {suffixOptions}. If no
      suffix is supplied, the value will be interpreted as {defaultUnits}.
      The value must be at least {minCacheSize} and less than
      {maxCachePlusOne}. Note that there is a memory overhead of 15%%.
      {defaultHelp}
                                 """)
      .format(pageSize = Defaults.vdoPhysicalBlockSize,
              suffixOptions = self.lvmOptionalSiSuffix,
              defaultUnits = Constants.lvmDefaultUnitsText,
              minCacheSize = Defaults.blockMapCacheSizeMin,
              maxCachePlusOne = Defaults.blockMapCacheSizeMaxPlusOne,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _blockMapPeriodOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault else
                   _("The default value is {0}.")
                      .format(Defaults.blockMapPeriod))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--blockMapPeriod",
                        type = self.__optionCheck(
                                  Defaults.checkBlockMapPeriod),
                        metavar = "<period>",
                        help = _("""
      Tunes the quantity of block map updates that can accumulate before
      cache pages are flushed to disk. The value must at least {minPeriod}
      and less than or equal to {maxPeriod}. A lower value means shorter
      recovery time but lower performance. {defaultHelp}
                                 """)
      .format(minPeriod = Defaults.blockMapPeriodMin,
              maxPeriod = Defaults.blockMapPeriodMax,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _commonOptionsParser(self):
    return argparse.ArgumentParser(add_help = False,
                                   parents = [self._confFileOptionParser(),
                                              self._debugOptionParser(),
                                              self._logFileOptionParser(),
                                              self._verboseOptionParser()])

  ####################################################################
  def _compressionOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--compression",
                        choices =  Constants.enableChoices,
                        default = Defaults.compression,
                        help = _("""
      Enables or disables compression when creating a VDO volume. The default
      is {compression}. Compression may be disabled if necessary to maximize
      performance or to speed processing of data that is unlikely to compress.
                                 """)
      .format(compression = Defaults.compression))

    return parser

  ####################################################################
  def _confFileOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("-f", "--confFile",
                        type = self.__optionCheck(Defaults.checkConfFile),
                        default = Defaults.confFile,
                        metavar = "<file>",
                        help = _("""
      Specifies an alternate configuration file; the default is {file}.
                                 """)
      .format(file = Defaults.confFile))

    return parser

  ####################################################################
  def _debugOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("-d", "--debug",
                        action = "store_true",
                        dest = "debug",
                        help = argparse.SUPPRESS)

    return parser

  ####################################################################
  def _deduplicationOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--deduplication",
                        choices =  Constants.enableChoices,
                        default = Defaults.deduplication,
                        help = _("""
      Enables or disables deduplication when creating a VDO volume. The
      default is {deduplication}. Deduplication may be disabled in instances
      where data is not expected to have good deduplication rates but
      compression is still desired.
                                 """)
      .format(deduplication = Defaults.deduplication))

    return parser

  ####################################################################
  def _deviceOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--device",
                        type = self.__optionCheck(Defaults.checkBlkDev),
                        metavar = "<devicepath>",
                        required = True,
                        help = _("""
      Specifies the absolute path of the device to use for VDO storage.
                                 """))

    return parser

  ####################################################################
  def _emulate512OptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--emulate512",
                        choices =  Constants.enableChoices,
                        help = _("""
      Specifies that the VDO volume is to emulate a 512 byte block device. The
      default is {emulate512}.
                                 """)
      .format(emulate512 = Defaults.emulate512))

    return parser

  ####################################################################
  def _forceOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--force",
                        action = "store_true",
                        dest = "force",
                        help = _("""
      When creating a volume, ignores any existing file system or VDO signature
      already present in the storage device. When stopping or removing a VDO
      volume, first unmounts the file system stored on the device if mounted.
                                 """))

    return parser

  ####################################################################
  def _forceRebuildOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--forceRebuild",
                        action = "store_true",
                        dest = "forceRebuild",
                        help = _("""
      Forces an offline rebuild of a read-only VDO's metadata before starting
      so that it may be brought back online and made available. This option
      may result in data loss or corruption.
                                 """))

    return parser

  ####################################################################
  def _indexMemOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--indexMem",
                        type = self.__optionCheck(Defaults.checkIndexmem),
                        default = Defaults.indexMem,
                        metavar = "<gigabytes>",
                        help = _("""
      Specifies the amount of index memory in gigabytes; the default is
      currently {indexMem} GB. The special decimal values 0.25, 0.5,
      0.75 can be used, as can any integer value at least {min} and less
      than or equal to {max}. (The special decimal values are matched as
      exact strings; "0.5" works but "0.50" is not accepted.)

      Larger values will require more disk space. For a dense index,
      each gigabyte of index memory will use approximately 11 GB of
      storage. For a sparse index, each gigabyte of index memory will
      use approximately 100 GB of storage.
                                 """)
                        .format(indexMem = Defaults.indexMem,
                                min = Defaults.indexMemIntMin,
                                max = Defaults.indexMemIntMax,
                              ))

    return parser

  ####################################################################
  def _logFileOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--logfile",
                        type = self.__optionCheck(Defaults.checkLogFile),
                        metavar = "<pathname>",
                        help = _("""
      Specify the path of the file to which log messages are directed. If
      unspecified, log messages will go to syslog. Warning and error messages
      are always logged to syslog.
                                 """))

    return parser

  ####################################################################
  def _maxDiscardSizeOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is provided nor mentioned in
                            the help text
    """
    defaultHelp = ("" if noDefault else
                   _("The default is {0}.").format(Defaults.maxDiscardSize))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--maxDiscardSize",
                        type = self.__optionCheck(Defaults.checkMaxDiscardSize),
                        metavar = "<megabytes>",
                        help = _("""
      Specifies the maximum discard size VDO can receive. This is used for
      performance tuning and support of devices above us. The value must be
      a multiple of {blockSize}. {suffixOptions}. If no suffix is supplied,
      the value will be interpreted as {defaultUnits}. The value must be at
      least {minDiscard} and less than {maxDiscardPlusOne}.
      {defaultHelp}
                                 """)
      .format(blockSize = Defaults.vdoPhysicalBlockSize,
              suffixOptions = self.lvmOptionalSiSuffix,
              defaultUnits = Constants.lvmDefaultUnitsText,
              minDiscard = Defaults.maxDiscardSizeMin,
              maxDiscardPlusOne = Defaults.maxDiscardSizeMaxPlusOne,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _nameOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    self.__parserAddNameOption(parser, required = True)

    return parser

  ####################################################################
  def _namingOptionsParser(self, required = True):
    """
    Arguments:
      required (boolean) - if True, an argument is required to this parser
    """
    parser = argparse.ArgumentParser(
              add_help = False).add_mutually_exclusive_group(
                                                        required = required)
    self.__parserAddAllOption(parser)
    self.__parserAddNameOption(parser)

    return parser

  ####################################################################
  def _sparseIndexOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--sparseIndex",
                        choices =  Constants.enableChoices,
                        help = _("""
      Enables sparse indexing. The default is {sparseIndex}.
                                 """)
      .format(sparseIndex = Defaults.sparseIndex))

    return parser

  ####################################################################
  def _uuidOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault else
                   "The default is {0}.".format(Defaults.uuid))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--uuid",
                        type = self.__optionCheck(Defaults.checkUUIDValue),
                        metavar = "<uuid>",
                        help = _("""
      Sets the UUID of the VDO volume. The value needs to be either a
      valid uuid or an empty string. If an empty string is specified, a
      new random uuid is generated for the VDO volume.
      {defaultHelp}
                                 """)
      .format(defaultHelp = defaultHelp))
    
    return parser

  ####################################################################
  def _vdoAckThreadsOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault else
                   "The default is {0}.".format(Defaults.ackThreads))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoAckThreads",
                        type = self.__optionCheck(
                                  Defaults.checkThreadCount0_100),
                        metavar = "<threadCount>",
                        help = _("""
      Specifies the number of threads to use for acknowledging completion of
      requested VDO I/O operations. The value must be at least {min} and less
      than or equal to {max}. {defaultHelp}
                                 """)
      .format(min = Defaults.ackThreadsMin,
              max = Defaults.ackThreadsMax,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _vdoBioRotationIntervalOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault else
                   "The default is {0}.".format(Defaults.bioRotationInterval))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoBioRotationInterval",
                        type = self.__optionCheck(
                                  Defaults.checkRotationInterval),
                        metavar = "<ioCount>",
                        help = _("""
      Specifies the number of I/O operations to enqueue for each
      bio-submission thread before directing work to the next. The value must
      be at least {min} and less than or equal to {max}. {defaultHelp}
                                 """)
      .format(min = Defaults.bioRotationIntervalMin,
              max = Defaults.bioRotationIntervalMax,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _vdoBioThreadsOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault else
                   _("The default is {0}.").format(Defaults.bioThreads))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoBioThreads",
                        type = self.__optionCheck(
                                  Defaults.checkThreadCount1_100),
                        metavar = "<threadCount>",
                        help = _("""
      Specifies the number of threads to use for submitting I/O operations to
      the storage device. The value must be at least {min} and less than or
      equal to {max}. Each additional thread after the first will use an
      additional {threadOverhead} MB of RAM {defaultHelp}
                                 """)
      .format(min = Defaults.bioThreadsMin,
              max = Defaults.bioThreadsMax,
              threadOverhead = Defaults.bioThreadOverheadMB,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _vdoCpuThreadsOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault else
                   _("The default is {0}.").format(Defaults.cpuThreads))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoCpuThreads",
                        type = self.__optionCheck(
                                  Defaults.checkThreadCount1_100),
                        metavar = "<threadCount>",
                        help = _("""
      Specifies the number of threads to use for CPU-intensive work such as
      hashing or compression. The value must be at least {min} and less than
      or equal to {max}. {defaultHelp}
                                 """)
      .format(min = Defaults.cpuThreadsMin,
              max = Defaults.cpuThreadsMax,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _vdoHashZoneThreadsOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault else
                   "The default is {0}.".format(Defaults.hashZoneThreads))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoHashZoneThreads",
                        type = self.__optionCheck(
                                  Defaults.checkThreadCount0_100),
                        metavar = "<threadCount>",
                        help = _("""
      Specifies the number of threads across which to subdivide parts of the
      VDO processing based on the hash value computed from the block data.
      The value must be at least {min} and less than or equal to {max}.
      vdoHashZonesThreads, vdoLogicalThreads and vdoPhysicalThreads must be
      either all zero or all non-zero. {defaultHelp}
                                 """)
      .format(min = Defaults.hashZoneThreadsMin,
              max = Defaults.hashZoneThreadsMax,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _vdoLogicalThreadsOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault else
                   "The default is {0}.".format(Defaults.hashZoneThreads))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoLogicalThreads",
                        type = self.__optionCheck(
                                  Defaults.checkLogicalThreadCount),
                        metavar = "<threadCount>",
                        help = _("""
      Specifies the number of threads across which to subdivide parts of the
      VDO processing based on the hash value computed from the block data.
      The value must be at least {min} and less than or equal to {max}. A
      logical thread count of {threshold} or more will require explicitly
      specifying a sufficiently large block map cache size, as well.
      vdoHashZonesThreads, vdoLogicalThreads and vdoPhysicalThreads must be
      either all zero or all non-zero. {defaultHelp}
                                 """)
      .format(min = Defaults.logicalThreadsMin,
              max = Defaults.logicalThreadsMax,
              threshold = Defaults.logicalThreadsBlockMapCacheSizeThreshold,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _vdoLogicalSizeOptionParser(self, required = False):
    """
    Arguments:
      required (boolean) - if True, no default is provided nor mentioned in
                            the help text
    """
    defaultHelp = ("" if required else
                   _("The default is the size of the storage device."))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoLogicalSize",
                        type = self.__optionCheck(Defaults.checkLogicalSize),
                        default = None if required else "0",
                        required = required,
                        metavar = "<megabytes>",
                        help = _("""
      Specifies the logical VDO volume size in {defaultUnits}.
      {suffixOptions}. Used for over-provisioning volumes. The maximum size
      supported is {max}. {defaultHelp}
                                 """)
      .format(defaultUnits = Constants.lvmDefaultUnitsText,
              suffixOptions = self.lvmOptionalSuffix,
              max = Defaults.logicalSizeMax,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _vdoLogLevelOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoLogLevel",
                        choices =  Defaults.vdoLogLevelChoices,
                        default = Defaults.vdoLogLevel,
                        help = _("""
      Specifies the VDO driver log level. Levels are case-sensitive. The
      default is {logLevel}.
                                 """)
      .format(logLevel = Defaults.vdoLogLevel))

    return parser

  ####################################################################
  def _vdoPhysicalThreadsOptionParser(self, noDefault = False):
    """
    Arguments:
      noDefault (boolean) - if True, no default is mentioned in the help text
    """
    defaultHelp = ("" if noDefault else
                   _("The default is {0}.").format(Defaults.physicalThreads))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoPhysicalThreads",
                        type = self.__optionCheck(
                                  Defaults.checkPhysicalThreadCount),
                        metavar = "<threadCount>",
                        help = _("""
      Specifies the number of threads across which to subdivide parts of the
      VDO processing based on physical block addresses. The value must be at
      least {min} and less than or equal to {max}. Each additional thread
      after the first will use an additional {overhead} MB of RAM.
      vdoPhysicalThreads, vdoHashZonesThreads and vdoLogicalThreads must be
      either all zero or all non-zero. {defaultHelp}
                                 """)
      .format(min = Defaults.physicalThreadsMin,
              max = Defaults.physicalThreadsMax,
              overhead = Defaults.physicalThreadOverheadMB,
              defaultHelp = defaultHelp))

    return parser

  ####################################################################
  def _vdoSlabSizeOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--vdoSlabSize",
                        type = self.__optionCheck(Defaults.checkSlabSize),
                        default = Defaults.slabSize,
                        metavar = "<megabytes>",
                        help = _("""
      Specifies the size of the increment by which a VDO is grown. Using a
      smaller size constrains the total maximum physical size that can be
      accommodated. Must be a power of two between {minSize} and {maxSize};
      the default is {defaultSlabSize}. {suffixOptions}. If no suffix is
      used, the value will be interpreted as {defaultUnits}.
                                 """)
      .format(minSize = Defaults.slabSizeMin,
              maxSize = Defaults.slabSizeMax,
              defaultSlabSize = Defaults.slabSize,
              suffixOptions = self.lvmOptionalSuffix,
              defaultUnits = Constants.lvmDefaultUnitsText))

    return parser

  ####################################################################
  def _verboseOptionParser(self):
    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--verbose",
                        action = "store_true",
                        dest = "verbose",
                        help = _("""
      Prints commands before executing them.
                                 """))

    return parser

  ####################################################################
  def _writePolicyOptionParser(self, required = False):
    """
    Arguments:
      required (boolean)  - If True, no default is provided or mentioned
                            in the help text.
    """
    defaultHelp = ("" if required else
                   _("The default is {0}.").format(Defaults.writePolicy))

    parser = argparse.ArgumentParser(add_help = False)
    parser.add_argument("--writePolicy",
                        choices =  Defaults.writePolicyChoices,
                        default = None if required else Defaults.writePolicy,
                        required = required,
                        help = _("""
      Specifies the write policy. 'sync' means writes are acknowledged only
      after data is on stable storage. 'sync' policy is not supported if the
      underlying storage is not also synchronous. 'async' means that writes
      are acknowledged when data has been cached for writing to stable
      storage; data which has not been flushed is not guaranteed to persist
      in this mode. 'auto' means that VDO will check the storage device
      and determine whether it supports flushes. If it does, VDO will run
      in async mode, otherwise it will run in sync mode.
      {defaultHelp}
                                 """)
      .format(defaultHelp = defaultHelp))

    return parser

  ####################################################################
  # Private methods
  ####################################################################
  @classmethod
  def __optionCheck(cls, checkFunc):
    """Returns a callable for option checking which traps and translates
    exceptions from the vdo management option checking to argparse's
    exception in order to provide consistent behavior for option errors
    including preventing stack traces.

    Arguments:
      checkFunc (callable)  - the vdo management option check method to use

    Returns:
      __checker (callable)  - the wrapper which translates vdo option check
                              exception to argparse's.
    """
    def __checker(optionValue):
      try:
        optionValue = checkFunc(optionValue)
      except ArgumentError as ex:
        raise argparse.ArgumentTypeError(str(ex))
      return optionValue

    return __checker

  ####################################################################
  def __parserAddAllOption(self, parser):
    parser.add_argument("-a", "--all",
                        action = "store_true",
                        dest = "all",
                        help = _("""
      Indicates that the command should be applied to all configured VDO
      volumes. May not be used with --name.
                                 """))

  ####################################################################
  def __parserAddNameOption(self, parser, required = False):
    parser.add_argument("-n", "--name",
                        type = self.__optionCheck(Defaults.checkVDOName),
                        metavar = "<volume>",
                        required = required,
                        help = _("""
    Operates on the specified VDO volume. May not be used with --all.
                                 """))

  ####################################################################
  def __postParseChecks(self, namespace):
    """Performs post-parsing checks between linked options.

    If there is a check failure invokes the appropriate parser error()
    method to display the error message and exit.

    Arguments:
      namespace (Namespace) - the Namespace instance populated by parsing
                              arguments
    """
    # For modify check that there was at least one option specified.
    if namespace.command == "modify":
      options = [option for option in self._changeableModifyOptions
                        if getattr(namespace, option, None) is not None]
      if len(options) == 0:
        self._modifyCommandParser.error(
          _("options must be specified from: {0}")
            .format("--{0}"
                    .format(", --".join(self._changeableModifyOptions))))

