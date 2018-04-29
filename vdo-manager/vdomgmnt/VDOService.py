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
  VDOService - manages the VDO service on the local node

  $Id: //eng/vdo-releases/magnesium-rhel7.5/src/python/vdo/vdomgmnt/VDOService.py#1 $

"""

from . import ArgumentError
from . import Constants
from . import Defaults
from . import MgmntLogger, MgmntUtils
from . import Service, ServiceError, ServiceStateError
from . import SizeString
from . import VDOKernelModuleService
from utils import Command, CommandError, runCommand
from utils import Transaction, transactional

import functools
import math
import os
import re
from socket import gethostbyname
import stat
import time
import yaml

########################################################################
class VDOServiceError(ServiceError):
  """Base class for VDO service exceptions.
  """
  ######################################################################
  # Overriden methods
  ######################################################################
  def __init__(self, msg = _("VDO volume error")):
    super(VDOServiceError, self).__init__(msg)

########################################################################
class VDOServiceExistsError(VDOServiceError):
  """VDO service exists exception.
  """
  ######################################################################
  # Overriden methods
  ######################################################################
  def __init__(self, msg = _("VDO volume exists")):
    super(VDOServiceExistsError, self).__init__(msg)

########################################################################
class VDOServicePreviousOperationError(VDOServiceError):
  """VDO volume previous operation was not completed.
  """
  ######################################################################
  # Overriden methods
  ######################################################################
  def __init__(self, msg = _("VDO volume previous operation is incomplete")):
    super(VDOServicePreviousOperationError, self).__init__(msg)

########################################################################
class VDOService(Service):
  """VDOService manages a vdo device mapper target on the local node.

  Attributes:
    ackThreads (int): Number of threads dedicated to performing I/O
      operation acknowledgement calls.
    activated (bool): If True, should be started by the `start` method.
    bioRotationInterval (int): Number of I/O operations to enqueue for
      one bio submission thread in a batch before moving on to enqueue for
      the next.
    bioThreads (int): Number of threads used to submit I/O operations to
      the storage device.
    blockMapCacheSize (sizeString): Memory allocated for block map pages.
    blockMapPeriod (int): Block map period.
    cpuThreads (int): Number of threads used for various CPU-intensive tasks
      such as hashing.
    device (path): The device used for storage for this VDO volume.
    enableCompression (bool): If True, compression should be
      enabled on this volume the next time the `start` method is run.
    enableDeduplication (bool): If True, deduplication should be
      enabled on this volume the next time the `start` method is run.
    enableReadCache (bool): If True, enables the VDO device's read cache.
    hashZoneThreads (int): Number of threads across which to subdivide parts
      of VDO processing based on the hash value computed from the block data
    indexCfreq (int): The Index checkpoint frequency.
    indexMemory (str): The Index memory setting.
    indexSparse (bool): If True, creates a sparse Index.
    indexThreads (int): The Index thread count. If 0, use a thread per core
    logicalSize (SizeString): The logical size of this VDO volume.
    logicalThreads (int): Number of threads across which to subdivide parts
      of the VDO processing based on logical block addresses.
    physicalSize (SizeString): The physical size of this VDO volume.
    physicalThreads (int): Number of threads across which to subdivide parts
      of the VDO processing based on physical block addresses.
    readCacheSize (SizeString): The size of the read cache, in addition
      to a minimum set by the VDO software.
    slabSize (SizeString): The size increment by which a VDO is grown. Using
      a smaller size constrains the maximum physical size that can be
      accomodated. Must be a power of two between 128M and 32G.
    writePolicy (str): sync, async or auto.
  """
  log = MgmntLogger.getLogger(MgmntLogger.myname + '.Service.VDOService')
  yaml_tag = u"!VDOService"

  # Key values to use accessing a dictionary created via yaml-loading the
  # output of vdo status.

  # Access the VDO list.
  vdosKey = "VDOs"

  # Access the per-VDO info.
  readCacheKey               = _("Read cache")
  readCacheSizeKey           = _("Read cache size")
  vdoAckThreadsKey           = _("Acknowledgement threads")
  vdoBioSubmitThreadsKey     = _("Bio submission threads")
  vdoBlockMapCacheSizeKey    = _("Block map cache size")
  vdoBlockMapPeriodKey       = _("Block map period")
  vdoBlockSizeKey            = _("Block size")
  vdoCompressionEnabledKey   = _("Compression")
  vdoCpuThreadsKey           = _("CPU-work threads")
  vdoDeduplicationEnabledKey = _("Deduplication")
  vdoHashZoneThreadsKey      = _("Hash zone threads")
  vdoLogicalSizeKey          = _("Logical size")
  vdoLogicalThreadsKey       = _("Logical threads")
  vdoMdRaid5ModeKey          = _("MD RAID5 mode")
  vdoPhysicalSizeKey         = _("Physical size")
  vdoPhysicalThreadsKey      = _("Physical threads")
  vdoStatisticsKey           = _("VDO statistics")
  vdoWritePolicyKey          = _("Write policy")

  # Options that cannot be changed for an already-created VDO device.
  # Specified as used by the command-line.
  fixedOptions = [ 'device' ]

  # Options that can be changed for an already-created VDO device,
  # though not necessarily while the device is running. The
  # command-line options and VDOService use different names,
  # hence the mapping.
  modifiableOptions = {
    'blockMapCacheSize'     : 'blockMapCacheSize',
    'blockMapPeriod'        : 'blockMapPeriod',
    'readCache'             : 'readCache',
    'readCacheSize'         : 'readCacheSize',
    'vdoAckThreads'         : 'ackThreads',
    'vdoBioRotationInterval': 'bioRotationInterval',
    'vdoBioThreads'         : 'bioThreads',
    'vdoCpuThreads'         : 'cpuThreads',
    'vdoHashZoneThreads'    : 'hashZoneThreads',
    'vdoLogicalThreads'     : 'logicalThreads',
    'vdoPhysicalThreads'    : 'physicalThreads',
  }

  # States in the process of constructing a vdo.
  class OperationState(object):
    beginCreate = 'beginCreate'
    beginGrowLogical = 'beginGrowLogical'
    beginGrowPhysical = 'beginGrowPhysical'
    beginRunningSetWritePolicy = 'beginRunningSetWritePolicy'
    finished = 'finished'
    unknown = 'unknown'

    ####################################################################
    @classmethod
    def specificOperationStates(cls):
      """Return a list of the possible specific operation states.

      "Specific operation state" means a state that is specifically set
      via normal processing.
      """
      return [cls.beginCreate, cls.beginGrowLogical, cls.beginGrowPhysical,
              cls.beginRunningSetWritePolicy, cls.finished]

  ######################################################################
  # Public methods
  ######################################################################
  @classmethod
  def validateModifiableOptions(cls, args):
    """Validates that any options specified in the arguments are solely
    those which are modifiable.

    Argument:
      args  - arguments passed from the user
    """
    for option in cls.fixedOptions:
      if getattr(args, option, None) is not None:
        msg = _("Cannot change option {0} after VDO creation").format(
                  option)
        raise ServiceError(msg)

  ######################################################################
  def activate(self):
    """Marks the VDO device as activated, updating the configuration.
    """
    self._handlePreviousOperationFailure()

    if self.activated:
      msg = _("{0} already activated").format(self.getName())
      self.log.announce(msg)
      return

    self.log.announce(_("Activating VDO {0}").format(self.getName()))
    self.activated = True
    self.config.addVdo(self.getName(), self, True)

  ######################################################################
  def announceReady(self, wasCreated=True):
    """Logs the VDO volume state during create/start."""
    if self.running():
      self.log.announce(_("VDO instance {0} volume is ready at {1}").format(
        self.getInstanceNumber(), self.getPath()))
    elif wasCreated:
      self.log.announce(_("VDO volume created at {0}").format(self.getPath()))
    elif not self.activated:
      self.log.announce(_("VDO volume cannot be started (not activated)"))

  ######################################################################
  def connect(self):
    """Connect to index."""
    self.log.announce(_("Attempting to get {0} to connect").format(
                        self.getName()))
    self._handlePreviousOperationFailure()

    runCommand(['dmsetup', 'message', self.getName(), '0', 'index-enable'])
    self.log.announce(_("{0} connect succeeded").format(self.getName()))

  ######################################################################
  @transactional
  def create(self, force = False):
    """Creates and starts a VDO target."""
    self.log.announce(_("Creating VDO {0}").format(self.getName()))
    self.log.debug("confFile is {0}".format(self.config.filepath))

    self._handlePreviousOperationFailure()

    if self.isConstructed:
      msg = _("VDO volume {0} already exists").format(self.getName())
      raise VDOServiceExistsError(msg)

    # Check that we have enough kernel memory to at least create the index.
    self._validateAvailableMemory(self.indexMemory);

    # Check that the hash zone, logical and physical threads are consistent.
    self._validateModifiableThreadCounts(self.hashZoneThreads,
                                         self.logicalThreads,
                                         self.physicalThreads)

    # Perform a verification that the storage device doesn't already
    # have something on it. We want to perform the same checks that
    # LVM does (which doesn't yet include checking for an
    # already-formatted VDO volume, but vdoformat does that), so we do
    # it by...actually making LVM do it for us!
    if not force:
      try:
        runCommand(['pvcreate', '-qq', '--test', self.device])
      except CommandError as e:
        # Messages from pvcreate aren't localized, so we can look at
        # the message generated and pick it apart. This will need
        # fixing if the message format changes or it gets localized.
        lines = e.getStandardError().splitlines()
        if ((len(lines) > 1)
            and (re.match(r"\s*TEST MODE", lines[0]) is not None)):
          detectionMatch = re.match(r"WARNING: (.* detected .*)\.\s+Wipe it\?",
                                    lines[1])
          if detectionMatch is not None:
            raise VDOServiceError('{0}; use --force to override'
                                  .format(detectionMatch.group(1)))
          # Skip the TEST MODE message and use the next one.
          e.setMessage(lines[1])
        # No TEST MODE message, just keep going.
        raise e

    # Make certain the kernel module is installed.
    self._installKernelModule(self.vdoLogLevel)

    # Do the create.
    self._setOperationState(self.OperationState.beginCreate)

    # As setting the operation state updates (and persists) the config file
    # we need to be certain to remove this instance if something goes wrong.
    transaction = Transaction.transaction()
    transaction.addUndoStage(self.config.persist)
    transaction.addUndoStage(functools.partial(self.config.removeVdo,
                                               self.getName()))

    self._constructVdoFormat(force)
    self._constructServiceStart()

    # The create is done.
    self._setOperationState(self.OperationState.finished)

  ######################################################################
  def deactivate(self):
    """Marks the VDO device as not activated, updating the configuration.
    """
    self._handlePreviousOperationFailure()

    if not self.activated:
      msg = _("{0} already deactivated").format(self.getName())
      self.log.announce(msg)
      return

    self.log.announce(_("Deactivating VDO {0}").format(self.getName()))
    self.activated = False
    self.config.addVdo(self.getName(), self, True)

  ######################################################################
  def disconnect(self):
    """Disables deduplication on this VDO device."""
    self._handlePreviousOperationFailure()

    try:
      runCommand(["dmsetup", "message", self.getName(), "0", "index-disable"])
    except Exception:
      self.log.error(_("Cannot stop deduplication on VDO {0}").format(
        self.getName()))
      raise

  ######################################################################
  def getInstanceNumber(self):
    """Returns the instance number of a vdo if running, or zero."""
    self._handlePreviousOperationFailure()

    if not self.instanceNumber and self.running():
      self._determineInstanceNumber()
    return self.instanceNumber

  ######################################################################
  def getPath(self):
    """Returns the full path to this VDO device."""
    return os.path.join("/dev/mapper", self.getName())

  ######################################################################
  @transactional
  def growLogical(self, newLogicalSize):
    """Grows the logical size of this VDO volume.

    Arguments:
      newLogicalSize (SizeString): The new size.
    """
    self._handlePreviousOperationFailure()

    if not self.running():
      msg = _("VDO volume {0} must be running").format(self.getName())
      raise ServiceError(msg)

    newLogicalSize.roundToBlock()
    if newLogicalSize < self.logicalSize:
      msg = _("Can't shrink a VDO volume (old size {0})").format(
              self.logicalSize)
      raise ServiceError(msg)
    elif newLogicalSize == self.logicalSize:
      msg = _("Can't grow a VDO volume by less than {0} bytes").format(
              Constants.VDO_BLOCK_SIZE)
      raise ServiceError(msg)

    # Do the grow.
    self._setOperationState(self.OperationState.beginGrowLogical)

    self.log.info(_("Preparing to increase logical size of VDO {0}").format(
      self.getName()))
    transaction = Transaction.transaction()
    transaction.setMessage(self.log.error,
                        _("Cannot prepare to grow logical on VDO {0}").format(
                          self.getName()))
    runCommand(["dmsetup", "message", self.getName(), "0",
                "prepareToGrowLogical", str(newLogicalSize.toBlocks())])
    transaction.setMessage(None)

    self._suspend()
    transaction.addUndoStage(self._resume)

    self.log.info(_("Increasing logical size of VDO volume {0}").format(
      self.getName()))
    transaction.setMessage(self.log.error,
                           _("Device {0} could not be changed").format(
                              self.getName()))
    numSectors = newLogicalSize.toSectors()
    vdoConf = self._generateModifiedDmTable(numSectors = str(numSectors))
    runCommand(["dmsetup", "reload", self._name, "--table", vdoConf])
    transaction.setMessage(None)
    self.log.info(_("Increased logical size of VDO volume {0}").format(
      self.getName()))
    self.logicalSize = newLogicalSize

    self._resume()

    # The grow is done.
    self._setOperationState(self.OperationState.finished)

  ######################################################################
  @transactional
  def growPhysical(self):
    """Grows the physical size of this VDO volume.

    Arguments:
      newPhysicalSize (SizeString): The new size. If None, use all the
                                    remaining free space in the volume
                                    group.
    """
    self._handlePreviousOperationFailure()

    if not self.running():
      msg = _("VDO volume {0} must be running").format(self.getName())
      raise ServiceError(msg)

    # Do the grow.
    self._setOperationState(self.OperationState.beginGrowPhysical)

    self.log.info(_("Preparing to increase physical size of VDO {0}").format(
      self.getName()))

    transaction = Transaction.transaction()
    transaction.setMessage(self.log.error,
                        _("Cannot prepare to grow physical on VDO {0}").format(
                            self.getName()))
    runCommand(["dmsetup", "message", self.getName(), "0",
                "prepareToGrowPhysical"])
    transaction.setMessage(None)

    self._suspend()
    transaction.addUndoStage(self._resume)

    transaction.setMessage(self.log.error,
                           _("Cannot grow physical on VDO {0}").format(
                               self.getName()))
    runCommand(['dmsetup', 'message', self.getName(), '0',
                'growPhysical'])
    transaction.setMessage(None)

    # Get the new physical size
    vdoConfig = self._getConfigFromVDO()
    sectorsPerBlock = vdoConfig["blockSize"] / Constants.SECTOR_SIZE
    physicalSize = vdoConfig["physicalBlocks"] * sectorsPerBlock
    self.physicalSize = SizeString("{0}s".format(physicalSize))

    self._resume()

    # The grow is done.
    self._setOperationState(self.OperationState.finished)

  ######################################################################
  def reconnect(self):
    """Enables deduplication on this VDO device."""
    self._handlePreviousOperationFailure()

    try:
      runCommand(["dmsetup", "message", self.getName(), "0", "index-enable"])
    except Exception:
      self.log.error(_("Cannot start deduplication on VDO {0}").format(
        self.getName()))
      raise

  ######################################################################
  def remove(self, force=False, removeSteps=None):
    """Removes a VDO target.

    If removeSteps is not None it as an empty list to which the processing
    commands for removal will be appended.

    If force was not specified and the instance previous operation failure
    is not recoverable VDOServicePreviousOperationError will be raised.
    """
    self.log.announce(_("Removing VDO {0}").format(self.getName()))

    localRemoveSteps = []
    try:
      self.stop(force, localRemoveSteps)

      # If we're not forcing handle any previous operation failure (which will
      # raise an exception if it's not handled).
      if not force:
        self._handlePreviousOperationFailure()
    except VDOServicePreviousOperationError:
      if (removeSteps is not None) and (len(localRemoveSteps) > 0):
        removeSteps.append(
          _("Steps to clean up VDO {0}:").format(self.getName()))
        removeSteps.extend(["    {0}".format(s) for s in localRemoveSteps])
      raise

    self.config.removeVdo(self.getName())

    # We delete the metadata after we remove the entry from the config
    # file because if we do it before and the removal from the config
    # fails, we will end up with a valid looking entry in the config
    # that has no valid metadata.
    self._clearMetadata()


  ######################################################################
  def running(self):
    """Returns True if the VDO service is available."""
    try:
      runCommand(["dmsetup", "status", self.getName()])
      return True
    except Exception:
      return False

  ######################################################################
  def start(self, forceRebuild=False):
    """Starts the VDO target mapper. In noRun mode, we always assume
    the service is not yet running.

    Raises:
      ServiceError
    """
    self.log.announce(_("Starting VDO {0}").format(self.getName()))

    self._handlePreviousOperationFailure()

    if not self.activated:
      self.log.info(_("VDO service {0} not activated").format(self.getName()))
      return
    if self.running() and not Command.noRunMode():
      self.log.info(_("VDO service {0} already started").format(
          self.getName()))
      return

    # Check that we have enough kernel memory to at least create the index.
    self._validateAvailableMemory(self.indexMemory);

    self._installKernelModule()
    self._checkConfiguration()

    try:
      if forceRebuild:
        try:
          self._forceRebuild()
        except Exception:
          self.log.error(_("Device {0} not read-only").format(self.getName()))
          raise

      runCommand(["dmsetup", "create", self._name, "--uuid", self._getUUID(),
                  "--table", self._generateDeviceMapperTable()])
      if not self.enableDeduplication:
        try:
          self.disconnect()
        except Exception:
          pass
      self._determineInstanceNumber()
      if self.instanceNumber:
        self.log.info(_("started VDO service {0} instance {1}").format(
          self.getName(), self.instanceNumber))

      try:
        if self.enableCompression:
          self._startCompression()
      except Exception:
        self.log.error(_("Could not enable compression for {0}").format(
            self.getName()))
        raise
    except Exception:
      self.log.error(_("Could not set up device mapper for {0}").format(
          self.getName()))
      raise

  ######################################################################
  def status(self):
    """Returns a dictionary representing the status of this object.
    """
    self._handlePreviousOperationFailure()

    status = {}
    status[_("Storage device")] = self.device
    status[self.vdoBlockMapCacheSizeKey] = str(self.blockMapCacheSize)
    status[self.vdoBlockMapPeriodKey] = self.blockMapPeriod
    status[self.vdoBlockSizeKey] = Constants.VDO_BLOCK_SIZE
    status[_("Emulate 512 byte")] = Constants.enableString(
                                      self.logicalBlockSize == 512)
    status[_("Activate")] = Constants.enableString(self.activated)
    status[self.readCacheKey] = Constants.enableString(self.enableReadCache)
    status[self.readCacheSizeKey] = str(self.readCacheSize)
    status[self.vdoCompressionEnabledKey] = Constants.enableString(
                                              self.enableCompression)
    status[self.vdoDeduplicationEnabledKey] = Constants.enableString(
                                                self.enableDeduplication)
    status[self.vdoLogicalSizeKey] = str(self.logicalSize)
    status[self.vdoPhysicalSizeKey] = str(self.physicalSize)
    status[self.vdoAckThreadsKey] = self.ackThreads
    status[self.vdoBioSubmitThreadsKey] = self.bioThreads
    status[_("Bio rotation interval")] = self.bioRotationInterval
    status[self.vdoCpuThreadsKey] = self.cpuThreads
    status[self.vdoHashZoneThreadsKey] = self.hashZoneThreads
    status[self.vdoLogicalThreadsKey] = self.logicalThreads
    status[self.vdoPhysicalThreadsKey] = self.physicalThreads
    status[_("Slab size")] = str(self.slabSize)
    status[_("Configured write policy")] = self.writePolicy
    status[_("Index checkpoint frequency")] = self.indexCfreq
    status[_("Index memory setting")] = self.indexMemory
    status[_("Index parallel factor")] = self.indexThreads
    status[_("Index sparse")] = Constants.enableString(self.indexSparse)
    status[_("Index status")] = self._getDeduplicationStatus()

    if os.getuid() == 0:
      status[_("Device mapper status")] = MgmntUtils.statusHelper(
                                            ['dmsetup', 'status',
                                             self.getName()])

      try:
        result = runCommand(['vdostats', '--verbose', self.getPath()])
        status[self.vdoStatisticsKey] = yaml.safe_load(result)
      except Exception:
        status[self.vdoStatisticsKey] = _("not available")

    return status

  ######################################################################
  def stop(self, force=False, removeSteps=None):
    """Stops the VDO target mapper. In noRun mode, assumes the service
    is already running.

    If removeSteps is not None it is a list to which the processing
    commands will be appended.

    If force was not specified and the instance previous operation failed
    VDOServicePreviousOperationError will be raised.

    Raises:
      ServiceError
      VDOServicePreviousOperationError
    """
    self.log.announce(_("Stopping VDO {0}").format(self.getName()))

    execute = force
    if not execute:
      try:
        self._handlePreviousOperationFailure()
        execute = True
      except VDOServicePreviousOperationError:
        pass

    if execute:
      if ((not self.running()) and (not Command.noRunMode())
          and (not self.previousOperationFailure)):
        self.log.info(_("VDO service {0} already stopped").format(
            self.getName()))
        return

    if self._hasMounts() or (not execute):
      command = ["umount", "-f", self.getPath()]
      if removeSteps is not None:
        removeSteps.append(" ".join(command))

      if execute:
        if force:
          runCommand(command, noThrow=True)
        else:
          msg = _("cannot stop VDO volume with mounts {0}").format(
                  self.getName())
          raise ServiceError(msg)

    # The udevd daemon can wake up at any time and use the blkid command on our
    # vdo device.  In fact, it can be triggered to do so by the unmount command
    # we might have just done.  Wait for udevd to process its event queue.
    command = ["udevadm", "settle"]
    if removeSteps is not None:
      removeSteps.append(" ".join(command))
    if execute:
      runCommand(command, noThrow=True)

    # In a modern Linux, we would use "dmsetup remove --retry".
    # But SQUEEZE does not have the --retry option.
    command = ["dmsetup", "remove", self.getName()]
    if removeSteps is not None:
      removeSteps.append(" ".join(command))

    if execute:
      for unused_i in range(10):
        try:
          runCommand(command)
          return
        except Exception as ex:
          if "Device or resource busy" not in str(ex):
            break
        time.sleep(1)

    # If we're not executing we're in a previous operation failure situation
    # and want to report that and go no further.
    if not execute:
      self._generatePreviousOperationFailureResponse()

    if self.running():
      msg = _("cannot stop VDO service {0}").format(self.getName())
      raise ServiceError(msg)

  ######################################################################
  def setCompression(self, enable):
    """Changes the compression setting on a VDO.  If the VDO is running
    the setting takes effect immediately.
    """
    self._announce("Enabling" if enable else "Disabling", "compression")
    self._handlePreviousOperationFailure()

    if ((enable and self.enableCompression) or
        ((not enable) and (not self.enableCompression))):
      message = "compression already {0} on VDO ".format(
                  "enabled" if enable else "disabled")
      self.log.info(_(message) + self.getName())
      return

    self.enableCompression = enable
    self.config.addVdo(self.getName(), self, True)

    if self.enableCompression:
      self._startCompression()
    else:
      self._stopCompression()

  ######################################################################
  def setConfig(self, config):
    """Sets the configuration reference and other attributes dependent on
    the configuration.

    This method must tolerate the possibility that the configuration is None
    to handle instantiation from YAML representation.  At present, there is
    nothing for which we attempt to use the configuration.
    """

    self._config = config
    self._configUpgraded = False

  ######################################################################
  def setDeduplication(self, enable):
    """Changes the deduplication setting on a VDO.  If the VDO is running
    the setting takes effect immediately.
    """
    self._announce("Enabling" if enable else "Disabling", "deduplication")
    self._handlePreviousOperationFailure()

    if ((enable and self.enableDeduplication) or
        ((not enable) and (not self.enableDeduplication))):
      message = "deduplication already {0} on VDO ".format(
                  "enabled" if enable else "disabled")
      self.log.info(_(message) + self.getName())
      return

    self.enableDeduplication = enable
    self.config.addVdo(self.getName(), self, True)

    if self.running():
      if self.enableDeduplication:
        self.reconnect()
        status = None
        for _i in range(Constants.DEDUPLICATION_TIMEOUT):
          status = self._getDeduplicationStatus()
          if status == Constants.deduplicationStatusOpening:
            time.sleep(1)
          else:
            break
        if status == Constants.deduplicationStatusOnline:
          pass
        elif status == Constants.deduplicationStatusError:
          raise ServiceError(_("Error enabling deduplication for {0}")
                             .format(self.getName()))
        elif status == Constants.deduplicationStatusOpening:
          message = (_("Timeout enabling deduplication for {0}, continuing")
                     .format(self.getName()))
          self.log.warn(message)
        else:
          message = (_("Unexpected kernel status {0} enabling deduplication for {0}")
                     .format(status, self.getName()))
          raise ServiceError(message)
      else:
        self.disconnect()

  ######################################################################
  def setModifiableOptions(self, args):
    """Sets any of the modifiable options that are specified in the arguments.

    Argument:
      args  - arguments passed from the user

    Raises:
      ArgumentError
    """
    self._handlePreviousOperationFailure()

    # Check that any modification to hash zone, logical and physical threads
    # maintain consistency.
    self._validateModifiableThreadCounts(getattr(args, "vdoHashZoneThreads",
                                                 self.hashZoneThreads),
                                         getattr(args, "vdoLogicalThreads",
                                                 self.logicalThreads),
                                         getattr(args, "vdoPhysicalThreads",
                                                 self.physicalThreads))

    modified = False
    for option in self.modifiableOptions:
      value = getattr(args, option, None)
      if value is not None:
        setattr(self, self.modifiableOptions[option], value)
        modified = True

    if modified:
      self.config.addVdo(self.getName(), self, True)

      if self.running():
        self.log.announce(
          _("Note: Changes will not apply until VDO {0} is restarted").format(
            self.getName()))

  ######################################################################
  def setWritePolicy(self, policy):
    """Changes the write policy on a VDO.  If the VDO is running it is
    restarted with the new policy"""
    self._handlePreviousOperationFailure()

    #pylint: disable=E0203
    if policy != self.writePolicy:
      self.writePolicy = policy

      if not self.running():
        self.config.addVdo(self.getName(), self, True)
      else:
        # Because the vdo is running we need to be able to handle recovery
        # should the user interrupt processing.
        # Setting the operation state will update the configuration thus
        # saving the specified state.
        self._setOperationState(self.OperationState.beginRunningSetWritePolicy)

        self._performRunningSetWritePolicy()

        # The setting of the write policy is finished.
        self._setOperationState(self.OperationState.finished)

  ######################################################################
  # Overridden methods
  ######################################################################
  @staticmethod
  def getKeys():
    """Returns the list of standard attributes for this object."""
    return ["ackThreads",
            "activated",
            "bioRotationInterval",
            "bioThreads",
            "blockMapCacheSize",
            "blockMapPeriod",
            "cpuThreads",
            "compression",
            "deduplication",
            "device",
            "hashZoneThreads",
            "indexCfreq",
            "indexMemory",
            "indexSparse",
            "indexThreads",
            "logicalBlockSize",
            "logicalSize",
            "logicalThreads",
            "_operationState",
            "physicalSize",
            "physicalThreads",
            "readCache",
            "readCacheSize",
            "slabSize",
            "writePolicy"]

  ######################################################################
  @classmethod
  def _yamlMakeInstance(cls):
    return cls("YAMLInstance", None)

  ######################################################################
  @property
  def _yamlData(self):
    data = super(VDOService, self)._yamlData
    data["activated"] = Constants.enableString(self.activated)
    data["blockMapCacheSize"] = str(self.blockMapCacheSize)
    data["compression"] = Constants.enableString(self.enableCompression)
    data["deduplication"] = Constants.enableString(self.enableDeduplication)
    data["indexSparse"] = Constants.enableString(self.indexSparse)
    data["logicalSize"] = str(self.logicalSize)
    data["physicalSize"] = str(self.physicalSize)
    data["readCache"] = Constants.enableString(self.enableReadCache)
    data["readCacheSize"] = str(self.readCacheSize)
    data["slabSize"] = str(self.slabSize)
    data["writePolicy"] = self.writePolicy
    return data

  ######################################################################
  def _yamlSetAttributes(self, attributes):
    super(VDOService, self)._yamlSetAttributes(attributes)
    self.activated = attributes["activated"] != Constants.disabled
    self.blockMapCacheSize = SizeString(attributes["blockMapCacheSize"])
    self.enableCompression = attributes["compression"] != Constants.disabled
    self.enableDeduplication = (attributes["deduplication"]
                                != Constants.disabled)
    self.indexSparse = attributes["indexSparse"] != Constants.disabled
    self.logicalSize = SizeString(attributes["logicalSize"])
    self.physicalSize = SizeString(attributes["physicalSize"])
    self.enableReadCache = attributes["readCache"] != Constants.disabled
    self.readCacheSize = SizeString(attributes["readCacheSize"])
    self.slabSize = SizeString(attributes["slabSize"])
    self.writePolicy = attributes["writePolicy"]

  ######################################################################
  @property
  def _yamlSpeciallyHandledAttributes(self):
    specials = super(VDOService, self)._yamlSpeciallyHandledAttributes
    specials.extend(["activated",
                     "blockMapCacheSize",
                     "compression",
                     "deduplication",
                     "indexSparse",
                     "logicalSize",
                     "physicalSize",
                     "readCache",
                     "readCacheSize",
                     "slabSize",
                     "writePolicy"])
    return specials

  ######################################################################
  def __getattr__(self, name):
    # Fake this attribute so we don't have to make incompatible
    # changes to the configuration file format.
    if name == "config":
      return self._computedConfig()
    elif name == "isConstructed":
      return self._computedIsConstructed()
    elif name == "operationState":
      return self._computedOperationState()
    elif name == "previousOperationFailure":
      return self._computedPreviousOperationFailure()
    elif name == "indexMemory":
      #pylint: disable=E1101
      return super(VDOService, self).__getattr__(name)
    elif name == "unrecoverablePreviousOperationFailure":
      return self._computedUnrecoverablePreviousOperationFailure()
    elif name == "writePolicy":
      return self._computedWritePolicy()
    else:
      raise AttributeError("'{obj}' object has no attribute '{attr}'".format(
          obj=type(self).__name__, attr=name))

  ######################################################################
  def __init__(self, name, conf, **kw):
    super(VDOService, self).__init__(name)

    self.setConfig(conf)

    # The state of operation of this instance is unknown.
    # It will either be updated from the config file as part of accessing
    # known vdos or it will be set during actual operation.
    self._operationState = self.OperationState.unknown
    self._previousOperationFailure = None

    # required value
    self.device = kw.get('device')

    self.ackThreads = self._defaultIfNone(kw, 'vdoAckThreads',
                                          Defaults.ackThreads)
    self.bioRotationInterval = self._defaultIfNone(
                                                  kw,
                                                  'vdoBioRotationInterval',
                                                  Defaults.bioRotationInterval)
    self.bioThreads = self._defaultIfNone(kw, 'vdoBioThreads',
                                          Defaults.bioThreads)
    self.blockMapCacheSize = self._defaultIfNone(kw, 'blockMapCacheSize',
                                                 Defaults.blockMapCacheSize)
    self.blockMapPeriod = self._defaultIfNone(kw, 'blockMapPeriod',
                                              Defaults.blockMapPeriod)
    self.cpuThreads = self._defaultIfNone(kw, 'vdoCpuThreads',
                                          Defaults.cpuThreads)
    self.logicalBlockSize = Constants.VDO_BLOCK_SIZE
    emulate512 = self._defaultIfNone(kw, 'emulate512', Defaults.emulate512)
    if emulate512 != Constants.disabled:
      self.logicalBlockSize = 512

    compression = self._defaultIfNone(kw, 'compression',
                                      Defaults.compression)
    self.enableCompression = (compression != Constants.disabled)
    deduplication = self._defaultIfNone(kw, 'deduplication',
                                        Defaults.deduplication)
    self.enableDeduplication = (deduplication != Constants.disabled)

    activate = self._defaultIfNone(kw, 'activate', Defaults.activate)
    self.activated = (activate != Constants.disabled)

    self.hashZoneThreads = self._defaultIfNone(kw, 'vdoHashZoneThreads',
                                               Defaults.hashZoneThreads)
    self.logicalSize = kw.get('vdoLogicalSize', SizeString("0"))
    self.logicalThreads = self._defaultIfNone(kw, 'vdoLogicalThreads',
                                              Defaults.logicalThreads)
    self.mdRaid5Mode = Defaults.mdRaid5Mode
    self.physicalSize = SizeString("0")
    self.physicalThreads = self._defaultIfNone(kw, 'vdoPhysicalThreads',
                                               Defaults.physicalThreads)
    readCache = self._defaultIfNone(kw, 'readCache', Defaults.readCache)
    self.enableReadCache = (readCache != Constants.disabled)
    self.readCacheSize = self._defaultIfNone(kw, 'readCacheSize',
                                             Defaults.readCacheSize)
    self.slabSize = self._defaultIfNone(kw, 'vdoSlabSize', Defaults.slabSize)
    self._writePolicy = self._defaultIfNone(kw, 'writePolicy',
                                            Defaults.writePolicy)
    self._writePolicySet = False  # track if the policy is explicitly set

    self.instanceNumber = 0

    self.vdoLogLevel = kw.get('vdoLogLevel')

    self.indexCfreq = self._defaultIfNone(kw, 'cfreq', Defaults.cfreq)
    self._setMemoryAttr(self._defaultIfNone(kw, 'indexMem',
                                            Defaults.indexMem))
    sparse = self._defaultIfNone(kw, 'sparseIndex', Defaults.sparseIndex)
    self.indexSparse = (sparse != Constants.disabled)
    self.indexThreads = self._defaultIfNone(kw, 'udsParallelFactor',
                                            Defaults.udsParallelFactor)

  ######################################################################
  def __setattr__(self, name, value):
    if name == "readCache":
      self.enableReadCache = value != Constants.disabled
    elif name == 'indexMemory':
      self._setMemoryAttr(value)
    elif name == "writePolicy":
      self._writePolicy = value
      self._writePolicySet = True
    elif name == 'identifier':
      # Setting the identifier must work, since we might have an old config
      # file with the identifier set, but we don't use it anymore so just
      # drop it. (It's deprecated.)
      pass
    else:
      super(VDOService, self).__setattr__(name, value)

    # We need to round all logical sizes and physical sizes.
    if name in ['logicalSize', 'physicalSize']:
      getattr(self, name).roundToBlock()

  ######################################################################
  # Protected methods
  ######################################################################
  @staticmethod
  def _defaultIfNone(args, name, default):
    value = args.get(name)
    return default if value is None else value

  ######################################################################
  def _announce(self, action, option):
    message = "{0} {1} on VDO ".format(action, option)
    self.log.announce(_(message) + self.getName())

  ######################################################################
  def _checkConfiguration(self):
    """Check and fix the configuration of this VDO.

    Raises:
      ServiceError
    """
    cachePages = self.blockMapCacheSize.toBlocks()
    if cachePages < 2 * 2048 * self.logicalThreads:
      msg = _("Insufficient block map cache for {0}").format(self.getName())
      raise ServiceError(msg)

    # Adjust the block map period to be in its acceptable range.
    self.blockMapPeriod = max(self.blockMapPeriod, Defaults.blockMapPeriodMin)
    self.blockMapPeriod = min(self.blockMapPeriod, Defaults.blockMapPeriodMax)

  ######################################################################
  def _clearMetadata(self):
    """Clear the VDO metadata from the storage device"""
    try:
      mode = os.stat(self.device).st_mode
      if not stat.S_ISBLK(mode):
        self.log.debug("Not clearing {devicePath}, not a block device".format(
          devicePath=self.device))
        return
    except OSError as ex:
        self.log.debug("Not clearing {devicePath}, cannot stat: {ex}".format(
          devicePath=self.device, ex=ex))
        return
    command = ["dd",
               "if=/dev/zero",
               "of={devicePath}".format(devicePath=self.device),
               "oflag=direct",
               "bs=4096",
               "count=1"]
    runCommand(command)

  ######################################################################
  def _computedConfig(self):
    """Update the instance properties as necessary and return the
    configuration instance.
    """
    # TODO: rework organization to avoid having to do local import
    from . import Configuration

    if not self._configUpgraded:
      # There may be an older existing entry in the config which needs to be
      # upgraded to account for attribute changes.
      service = None
      try:
        service = self._config.getVdo(self.getName())
      except ArgumentError:
        pass
      else:
        # Getting here means we found an entry in the configuration.
        # Upgrade the entry as necessary.
        #
        # If the entry is in the 'unknown' state that indicates that it is
        # a pre-existing instance before we added the operation state to
        # the configuration. These are to be treated as finished, but we
        # don't have to force the update to disk.
        if service._operationState == self.OperationState.unknown:
          service._setOperationState(self.OperationState.finished,
                                     persist=False)

      self._configUpgraded = True

    return self._config

  ######################################################################
  def _computedIsConstructed(self):
    """Returns a boolean indicating if the instance represents a fully
    constructed vdo.
    """
    return self.operationState == self.OperationState.finished

  ######################################################################
  def _computedOperationState(self):
    """Return the operation state of the instance.

    If there is an instance in the configuration the state reported is from
    that instance else it's from this instance.
    """
    service = self
    try:
      service = self.config.getVdo(self.getName())
    except ArgumentError:
      pass

    return service._operationState

  ######################################################################
  def _computedPreviousOperationFailure(self):
    """Returns a boolean indicating if the instance operation failed.
    """
    if self._previousOperationFailure is None:
      # We access the operation state property in order to determine
      # the operation failure status as that will give us the actual
      # operation state of the existing (if so) entry in the config which
      # represents the real operation state.
      self._previousOperationFailure = ((self.operationState !=
                                         self.OperationState.unknown)
                                        and (self.operationState !=
                                              self.OperationState.finished))

    return self._previousOperationFailure

  ######################################################################
  def _computedUnrecoverablePreviousOperationFailure(self):
    """Returns a boolean indicating if a previous operation failure cannot be
    automatically recovered.
    """
    return (self.previousOperationFailure
            and (self.operationState == self.OperationState.beginCreate))

  ######################################################################
  def _computedWritePolicy(self):
    """Return the write policy of the instance.

    If this instance's write policy was not explicitly set and there is an
    instance in the configuration the write policy reported is from that
    instance else it's from this instance.
    """
    service = self
    if not self._writePolicySet:
      try:
        service = self.config.getVdo(self.getName())
      except ArgumentError:
        pass

    return service._writePolicy

  ######################################################################
  def _computeSlabBits(self):
    """Compute the --slab-bits parameter value for the slabSize attribute."""
    # add some fudge because of imprecision in long arithmetic
    blocks = self.slabSize.toBlocks()
    return int(math.log(blocks, 2) + 0.05)

  ######################################################################
  def _constructServiceStart(self):
    self.log.debug("construction - starting; vdo {0}".format(self.getName()))

    transaction = Transaction.transaction()
    self.start()
    transaction.addUndoStage(self.stop)

  ######################################################################
  def _constructVdoFormat(self, force = False):
    self.log.debug("construction - formatting logical volume; vdo {0}"
                    .format(self.getName()))

    transaction = Transaction.transaction()
    self._formatTarget(force)
    transaction.addUndoStage(self.remove)

    vdoConfig = self._getConfigFromVDO()
    sectorsPerBlock = vdoConfig["blockSize"] / Constants.SECTOR_SIZE
    physicalSize = vdoConfig["physicalBlocks"] * sectorsPerBlock
    self.physicalSize = SizeString("{0}s".format(physicalSize))
    logicalSize = vdoConfig["logicalBlocks"] * sectorsPerBlock
    self.logicalSize = SizeString("{0}s".format(logicalSize))

  ######################################################################
  def _determineInstanceNumber(self):
    """Determine the instance number of a running VDO using sysfs."""
    path = "/sys/kvdo/{0}/instance".format(self.getName())
    try:
      with open(path, "r") as f:
        self.instanceNumber = int(f.read().strip())
    except Exception as err:
      self.log.warning(
        _("unable to determine VDO service {0} instance number: {1}").format(
          self.getName(), err))

  ######################################################################
  def _forceRebuild(self):
    """Calls vdoforcerebuild to exit read-only mode and force a metadata
    rebuild at next start.
    """
    runCommand(['vdoforcerebuild', self.device])

  ######################################################################
  def _formatTarget(self, force):
    """Formats the VDO target."""
    commandLine = ['vdoformat']
    commandLine.append("--uds-checkpoint-frequency=" + str(self.indexCfreq))

    memVal = self.indexMemory
    if memVal == 0.0:
      memVal = Defaults.indexMem

    if not self.slabSize:
      self.slabSize = Defaults.slabSize

    commandLine.append("--uds-memory-size=" + str(memVal))
    if self.indexSparse:
      commandLine.append("--uds-sparse")
    if self.logicalSize.toBytes() > 0:
      commandLine.append("--logical-size=" + self.logicalSize.asLvmText())
    if self.physicalSize.toBytes() > 0:
      commandLine.append("--physical-size=" + self.physicalSize.asLvmText())
    if self.slabSize != Defaults.slabSize:
      commandLine.append("--slab-bits=" + str(self._computeSlabBits()))
    if force:
      commandLine.append("--force")
    commandLine.append(self.device)
    runCommand(commandLine)

  ######################################################################
  def _generateDeviceMapperTable(self):
    """Generate the device mapper table line from the properties of this
    object.
    """
    numSectors = self.logicalSize.toSectors()
    cachePages = self.blockMapCacheSize.toBlocks()
    threadCountConfig = ",".join(["ack=" + str(self.ackThreads),
                                  "bio=" + str(self.bioThreads),
                                  ("bioRotationInterval="
                                   + str(self.bioRotationInterval)),
                                  "cpu=" + str(self.cpuThreads),
                                  "hash=" + str(self.hashZoneThreads),
                                  "logical=" + str(self.logicalThreads),
                                  "physical=" + str(self.physicalThreads)])
    vdoConf = " ".join(["0", str(numSectors), Defaults.vdoTargetName,
                        self.device,
                        str(self.logicalBlockSize),
                        Constants.enableString(self.enableReadCache),
                        str(self.readCacheSize.toBlocks()),
                        str(cachePages), str(self.blockMapPeriod),
                        self.mdRaid5Mode, self.writePolicy,
                        self._name,
                        threadCountConfig])
    return vdoConf

  ######################################################################
  def _generateModifiedDmTable(self, **kwargs):
    """Changes the specified parameters in the dmsetup table to the specified
    values.

    Raises:
      CommandError if the current table cannot be obtained

    Returns:
      a valid new dmsetup table.
    """
    table = runCommand(["dmsetup", "table", self._name]).rstrip()
    self.log.info(table)

    # Parse the existing table.
    tableOrder = ("logicalStart numSectors targetName storagePath blockSize"
                  + " readCache readCacheBlocks cacheBlocks blockMapPeriod"
                  + " mdRaid5Mode writePolicy poolName"
                  + " threadCountConfig")

    dmTable = dict(zip(tableOrder.split(" "), table.split(" ")))

    # Apply new values
    for (key, val) in kwargs.iteritems():
      dmTable[key] = val

    # Create and return the new table
    return " ".join([dmTable[key] for key in tableOrder.split(" ")])

  ######################################################################
  def _generatePreviousOperationFailureResponse(self, operation = "create"):
    """Generates the required response to a previous operation failure.

    Logs a message indicating that the previous operation failed and raises the
    VDOServicePreviousOperationError exception with the same message.

    Arguments:
      operation (str) - the operation that failed; default to "create" as that
                        is currently the only operation that is not
                        automatically recovered

    Raises:
      VDOServicePreviousOperationError
    """
    msg = _("VDO volume {0} previous operation ({1}) is incomplete{2}").format(
            self.getName(), operation,
            "; recover by performing 'remove --force'"
              if operation == "create" else "")
    raise VDOServicePreviousOperationError(msg)

  ######################################################################
  def _handlePreviousOperationFailure(self):
    """Handles a previous operation failure.

    If the failure can be corrected automatically it is.
    If not, the method logs a message indicating that the previous operation
    failed and raises the VDOServicePreviousOperationError exception with
    the same message.

    Raises:
      VDOServicePreviousOperationError if the previous operation failure
      is a non-recoverable error.

      VDOServiceError if unexpected/unhandled operation state is
      encountered.  Excepting corruption or incorrect setting of state this
      indicates that the developer augmented the code with an operation (new or
      old) which can experience a catastrophic failure requiring some form of
      recovery but failed to update specificOperationStates() and/or failed to
      add a clause to this method to address the new failure.
    """

    if not self.previousOperationFailure:
      self.log.debug(
        _("No failure requiring recovery for VDO volume {0}").format(
          self.getName()))
      return

    if (self.operationState
        not in self.OperationState.specificOperationStates()):
      msg = _("VDO volume {0} in unknown operation state: {1}").format(
              self.getName(), self.operationState)
      raise VDOServiceError(msg)
    elif self.operationState == self.OperationState.beginCreate:
      # Create is not automatically recovered.
      self._generatePreviousOperationFailureResponse()
    elif self.operationState == self.OperationState.beginGrowLogical:
      self._recoverGrowLogical()
    elif self.operationState == self.OperationState.beginGrowPhysical:
      self._recoverGrowPhysical()
    elif self.operationState == self.OperationState.beginRunningSetWritePolicy:
      self._recoverRunningSetWritePolicy()
    else:
      msg = _("Missing handler for recover from operation state: {0}").format(
              self.operationState)
      raise VDOServiceError(msg)

    self._previousOperationFailure = False

  ######################################################################
  def _getBaseDevice(self, devicePath):
    """Take the server name and convert it to a device name that
    can be opened from the kernel

    Arguments:
      devicePath (path): path to a device.

    Raises:
      ArgumentError
    """
    resolvedPath = devicePath
    if not os.access(resolvedPath, os.F_OK):
      raise ArgumentError(
              _("{path} does not exist").format(path = resolvedPath))
    if stat.S_ISLNK(os.lstat(resolvedPath).st_mode):
      cmd = Command(["readlink", "-f", resolvedPath])
      resolvedPath = cmd.run().strip()
    if resolvedPath == "":
      raise ArgumentError(
              _("{path} could not be resolved").format(path = resolvedPath))
    return resolvedPath

  ######################################################################
  def _getConfigFromVDO(self):
    """Returns a dictionary of the configuration values as reported from
    the actual vdo storage.
    """
    config = yaml.safe_load(runCommand(["vdodumpconfig",
                                        self.device]))
    return config["VDOConfig"]

  ######################################################################
  def _getUUID(self):
    """Returns the uuid as reported from the actual vdo storage.
    """
    config = yaml.safe_load(runCommand(["vdodumpconfig",
                                        self.device]))
    return "VDO-" + config["UUID"]

  ######################################################################
  def _getDeduplicationStatus(self):
    status = _("not available")

    try:
      output = runCommand(["dmsetup", "status", self.getName()])
      fields = output.split(" ")
      status = fields[Constants.dmsetupStatusFields.deduplicationStatus]
    except Exception:
      pass

    return status

  ######################################################################
  def _hasMounts(self):
    """Tests whether filesystems are mounted on the VDO device.

    Returns:
      True iff the VDO device has something mounted on it.
    """
    mountList = runCommand(['mount'], noThrow=True)
    if mountList:
      matcher = re.compile(r'(\A|\s+)' + re.escape(self.getPath()) + r'\s+')
      for line in mountList.splitlines():
        if matcher.search(line):
          return True
    return False

  ######################################################################
  def _installKernelModule(self, logLevel = None):
    """Install the kernel module providing VDO support.

    Arguments:
      logLevel: the level of logging to use; if None, do not set level
    """
    kms = VDOKernelModuleService()
    try:
      kms.start()
    except Exception:
      self.log.error(_("Kernel module {0} not installed").format(
          kms.getName()))
      raise
    if logLevel is not None:
      kms.setLogLevel(logLevel)

  ######################################################################
  @transactional
  def _performRunningSetWritePolicy(self):
    """Peforms the changing of the write policy on a running vdo instance.
    """
    transaction = Transaction.transaction()
    self._suspend()
    transaction.addUndoStage(self._resume)

    transaction.setMessage(self.log.error,
                           _("Device {0} could not be read").format(
                                                          self.getName()))
    vdoConf = self._generateModifiedDmTable(writePolicy = self.writePolicy)

    transaction.setMessage(self.log.error,
                           _("Device {0} could not be changed").format(
                                                          self.getName()))
    runCommand(["dmsetup", "reload", self._name, "--table", vdoConf])
    transaction.setMessage(None)
    self._resume()

  ######################################################################
  def _recoverGrowLogical(self):
    """Recovers a VDO target from a previous grow logical failure.

    Raises:
      VDOServiceError
    """
    if not self.previousOperationFailure:
      self.log.debug(
        _("No grow logical recovery necessary for VDO volume {0}").format(
          self.getName()))
    elif self.operationState != self.OperationState.beginGrowLogical:
      msg = _("Previous operation failure for VDO volume {0} not from"
              " grow logical").format(self.getName())
      raise VDOServiceError(msg)
    else:
      # Get the correct logical size from vdo.
      vdoConfig = self._getConfigFromVDO()
      logicalSize = (vdoConfig["logicalBlocks"]
                      * (vdoConfig["blockSize"] / Constants.SECTOR_SIZE))
      self.logicalSize = SizeString("{0}s".format(logicalSize))

      # If vdo is running it's possible the failure came from the user
      # interrupting the original command so we issue a resume.
      # This is safe even if not necessary.
      if self.running():
        self._resume()

      # Mark the operation as finished (which also updates and persists the
      # configuration.
      self._setOperationState(self.OperationState.finished)

  ######################################################################
  def _recoverGrowPhysical(self):
    """Recovers a VDO target from a previous grow physical failure.

    Raises:
      VDOServiceError
    """
    if not self.previousOperationFailure:
      self.log.debug(
        _("No grow physical recovery necessary for VDO volume {0}").format(
          self.getName()))
    elif self.operationState != self.OperationState.beginGrowPhysical:
      msg = _("Previous operation failure for VDO volume {0} not from"
              " grow physical").format(self.getName())
      raise VDOServiceError(msg)
    else:
      # Get the correct physical size from vdo.
      vdoConfig = self._getConfigFromVDO()
      physicalSize = (vdoConfig["physicalBlocks"]
                      * (vdoConfig["blockSize"] / Constants.SECTOR_SIZE))
      self.physicalSize = SizeString("{0}s".format(physicalSize))

      # If vdo is running it's possible the failure came from the user
      # interrupting the original command so we issue a resume.
      # This is safe even if not necessary.
      if self.running():
        self._resume()

      # Mark the operation as finished (which also updates and persists the
      # configuration.
      self._setOperationState(self.OperationState.finished)

  ######################################################################
  def _recoverRunningSetWritePolicy(self):
    """Recovers a VDO target from a previous setting of write policy against
    a running VDO.

    Raises:
      VDOServiceError
    """
    if not self.previousOperationFailure:
      self.log.debug(
        _("No set write policy recovery necessary for VDO volume {0}").format(
          self.getName()))
    elif self.operationState != self.OperationState.beginRunningSetWritePolicy:
      msg = _("Previous operation failure for VDO volume {0} not from"
              " set write policy").format(self.getName())
      raise VDOServiceError(msg)
    else:
      # Perform the recovery only if the vdo is actually running (indicating
      # the user aborted the command).
      # If the vdo is not running the value stored in the configuration is what
      # we want to use and it will be used when starting the vdo.
      # In both cases we can go ahead and mark the operation as finished.
      if self.running():
        self._performRunningSetWritePolicy()

      # Mark the operation as finished (which also updates and persists the
      # configuration.
      self._setOperationState(self.OperationState.finished)

  ######################################################################
  def _resume(self):
    """Resumes a suspended VDO."""
    self.log.info(_("Resuming VDO volume {0}").format(self.getName()))
    try:
      runCommand(["dmsetup", "resume", self.getName()])
    except Exception as ex:
      self.log.error(_("Can't resume VDO volume {0}; {1!s}").format(
          self.getName(), ex))
      raise
    self.log.info(_("Resumed VDO volume {0}").format(self.getName()))

  ######################################################################
  def _setMemoryAttr(self, value):
    memory = float(value)
    if memory >= 1.0:
      memory = int(memory)
    # Call the superclass __setattr__ as this method is called from
    # our __setattr__ and we need to avoid an infinite recursion.
    super(VDOService, self).__setattr__("indexMemory", memory)

  ######################################################################
  def _setOperationState(self, state, persist=True):
    self._operationState = state
    if persist:
      self.config.addVdo(self.getName(), self, replace = True)
      self.config.persist()

  ######################################################################
  def _startCompression(self):
    """Starts compression on a VDO volume if it is running.
    """
    self._toggleCompression(True)

  ######################################################################
  def _stopCompression(self):
    """Stops compression on a VDO volume if it is running.
    """
    self._toggleCompression(False)

  ######################################################################
  def _suspend(self):
    """Suspends a running VDO."""
    self.log.info(_("Suspending VDO volume {0}").format(self.getName()))
    try:
      runCommand(["dmsetup", "suspend", self.getName()])
    except Exception as ex:
      self.log.error(_("Can't suspend VDO volume {0}; {1!s}").format(
          self.getName(), ex))
      raise
    self.log.info(_("Suspended VDO volume {0}").format(self.getName()))

  ######################################################################
  def _toggleCompression(self, enable):
    """Turns compression on or off if the VDO is running.

    Arguments:
      enable (boolean): True if compression should be enabled
    """
    if not self.running():
      return

    self.log.announce(_("{0} compression on VDO {1}").format(
        "Starting" if enable else "Stopping", self.getName()))
    runCommand(["dmsetup", "message", self.getName(), "0",
                "compression", "on" if enable else "off"])

  ######################################################################
  def _validateAvailableMemory(self, indexMemory):
    """Validates whether there is likely enough kernel memory to at least 
    create the index. If there is an error getting the info, don't 
    fail the create, just let the real check be done in vdoformat.

    Arguments:
      indexMemory - the amount of memory requested or default.

    Raises:
      ArgumentError
    """
    memoryNeeded = SizeString("{0}g".format(indexMemory))
    memoryAvailable = None
    try:
      result = runCommand(['grep', 'MemAvailable', '/proc/meminfo'])
      for line in result.splitlines():
        memory = re.match(r"MemAvailable:\s*(\d+)", line)
        if memory is not None:
          available = memory.group(1)
          memoryAvailable = SizeString("{0}k".format(available))
    except Exception:
      pass

    if memoryAvailable is None:
      self.log.info("Unable to validate available memory")
      return;

    if (memoryNeeded.toBytes() >= memoryAvailable.toBytes()):
      raise ArgumentError(_("Not enough available memory in system"
                            " for index requirement of {needed}".format(
                              needed = memoryNeeded)));

  ######################################################################
  def _validateModifiableThreadCounts(self, hashZone, logical, physical):
    """Validates that the hash zone, logical and physical thread counts
    are consistent (all zero or all non-zero).

    Arguments:
      hashZone  - hash zone thread count to use, may be None
      logical   - logical thread count to use, may be None
      physical  - physical thread count to use, may be None

    Raises:
      ArgumentError
    """
    if hashZone is None:
      hashZone = self.hashZoneThreads
    if logical is None:
      logical = self.logicalThreads
    if physical is None:
      physical = self.physicalThreads

    if (((hashZone == 0) or (logical == 0) or (physical == 0))
        and (not ((hashZone == 0) and (logical == 0) and (physical == 0)))):
      raise ArgumentError(_("hash zone, logical and physical threads must"
                            " either all be zero or all be non-zero"))
