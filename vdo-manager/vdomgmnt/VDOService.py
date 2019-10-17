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

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/VDOService.py#28 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from . import ArgumentError
from . import Constants
from . import Defaults
from . import MgmntUtils
from . import Service, ServiceError
from . import SizeString
from . import VDOKernelModuleService
from . import DeveloperExitStatus, StateExitStatus
from . import SystemExitStatus, UserExitStatus
from vdo.utils import Command, CommandError, runCommand
from vdo.utils import Transaction, transactional

import functools
import locale
import logging
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
  def __init__(self, msg = _("VDO volume error"), *args, **kwargs):
    super(VDOServiceError, self).__init__(msg, *args, **kwargs)

########################################################################
class VDODeviceAlreadyConfiguredError(UserExitStatus, VDOServiceError):
  """The specified device is already configured for a VDO.
  """
  ######################################################################
  # Overriden methods
  ######################################################################
  def __init__(self, msg = _("Device already configured"), *args, **kwargs):
    super(VDODeviceAlreadyConfiguredError, self).__init__(msg, *args, **kwargs)

########################################################################
class VDOServiceExistsError(UserExitStatus, VDOServiceError):
  """VDO service exists exception.
  """
  ######################################################################
  # Overriden methods
  ######################################################################
  def __init__(self, msg = _("VDO volume exists"), *args, **kwargs):
    super(VDOServiceExistsError, self).__init__(msg, *args, **kwargs)

########################################################################
class VDOMissingDeviceError(StateExitStatus, VDOServiceError):
  """VDO underlying device does not exist exception.
  """
  ######################################################################
  # Overriden methods
  ######################################################################
  def __init__(self, msg = _("Underlying device does not exist"),
               *args, **kwargs):
    super(VDOMissingDeviceError, self).__init__(msg, *args, **kwargs)

########################################################################
class VDOServicePreviousOperationError(StateExitStatus, VDOServiceError):
  """VDO volume previous operation was not completed.
  """
  ######################################################################
  # Overriden methods
  ######################################################################
  def __init__(self, msg = _("VDO volume previous operation is incomplete"),
               *args, **kwargs):
    super(VDOServicePreviousOperationError, self).__init__(msg,
                                                           *args, **kwargs)

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
    hashZoneThreads (int): Number of threads across which to subdivide parts
      of VDO processing based on the hash value computed from the block data
    indexCfreq (int): The Index checkpoint frequency.
    indexMemory (str): The Index memory setting.
    indexSparse (bool): If True, creates a sparse Index.
    indexThreads (int): The Index thread count. If 0, use a thread per core
    logicalSize (SizeString): The logical size of this VDO volume.
    logicalThreads (int): Number of threads across which to subdivide parts
      of the VDO processing based on logical block addresses.
    maxDiscardSize (SizeString): the max discard size for this VDO volume.
    physicalSize (SizeString): The physical size of this VDO volume.
    physicalThreads (int): Number of threads across which to subdivide parts
      of the VDO processing based on physical block addresses.
    slabSize (SizeString): The size increment by which a VDO is grown. Using
      a smaller size constrains the maximum physical size that can be
      accomodated. Must be a power of two between 128M and 32G.
    uuid (str): uuid of vdo volume.
    writePolicy (str): sync, async or auto.
  """
  log = logging.getLogger('vdo.vdomgmnt.Service.VDOService')
  yaml_tag = "!VDOService"
  
  # Key values to use accessing a dictionary created via yaml-loading the
  # output of vdo status.

  # Access the VDO list.
  vdosKey = "VDOs"

  # Access the per-VDO info.
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
  vdoMaxDiscardSizeKey       = _("Max discard size")
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
    'maxDiscardSize'        : 'maxDiscardSize',
    'uuid'                  : 'uuid',
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
    beginImport = 'beginImport'
    beginRunningSetWritePolicy = 'beginRunningSetWritePolicy'
    finished = 'finished'
    unknown = 'unknown'
    names = { beginCreate                : 'create',
              beginGrowLogical           : 'grow logical',
              beginGrowPhysical          : 'grow physical',
              beginImport                : 'import',
              beginRunningSetWritePolicy : 'write policy',
              finished                   : 'unknown',
              unknown                    : 'unknown' }
              

    ####################################################################
    @classmethod
    def specificOperationStates(cls):
      """Return a list of the possible specific operation states.

      "Specific operation state" means a state that is specifically set
      via normal processing.
      """
      return [cls.beginCreate, cls.beginGrowLogical, cls.beginGrowPhysical,
              cls.beginImport, cls.beginRunningSetWritePolicy, cls.finished]
      
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
        raise ServiceError(msg, exitStatus = UserExitStatus)
    # Can't use --all and --uuid with a specific uuid
    if getattr(args, "uuid", None) is not None:
      if args.uuid != "" and args.all:
        msg = _("Cannot change uuid to a specific value when --all is"
                " set").format(option)
        raise ServiceError(msg, exitStatus = UserExitStatus)

  ######################################################################
  def activate(self):
    """Marks the VDO device as activated, updating the configuration.
    """
    self._handlePreviousOperationFailure()

    if self.activated:
      msg = _("{0} already activated").format(self.getName())
      self._announce(msg)
      return

    self._announce(_("Activating VDO {0}").format(self.getName()))
    self.activated = True
    self.config.addVdo(self.getName(), self, True)

  ######################################################################
  def announceReady(self, wasCreated=True):
    """Logs the VDO volume state during create/start."""
    if self.running():
      self._announce(_("VDO instance {0} volume is ready at {1}").format(
        self.getInstanceNumber(), self.getPath()))
    elif wasCreated:
      self._announce(_("VDO volume created at {0}").format(
        self.getPath()))
    elif not self.activated:
      self._announce(_("VDO volume cannot be started (not activated)"))

  ######################################################################
  def connect(self):
    """Connect to index."""
    self._announce(_("Attempting to get {0} to connect").format(
      self.getName()))
    self._handlePreviousOperationFailure()

    runCommand(['dmsetup', 'message', self.getName(), '0', 'index-enable'])
    self._announce(_("{0} connect succeeded").format(self.getName()))

  ######################################################################
  @transactional
  def create(self, force = False):
    """Creates and starts a VDO target."""
    self._announce(_("Creating VDO {0}").format(self.getName()))
    self.log.debug("confFile is {0}".format(self.config.filepath))

    self._handlePreviousOperationFailure()

    # Check for various creation issues.
    self._checkForExistingVDO()

    # Validate some parameters.
    self._validateParameters()

    # Perform a verification that the storage device doesn't already
    # have something on it.
    if not force:
      self._createCheckCleanDevice()

    # Check to see if any other known VDO volume has the same UUID
    self._checkForExistingUUID(self.uuid)
    
    # Find a stable name for the storage device.
    self._setStableName()

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
    self._setUUID(self.uuid)
      
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
      self._announce(msg)
      return

    self._announce(_("Deactivating VDO {0}").format(self.getName()))
    self.activated = False
    self.config.addVdo(self.getName(), self, True)

  ######################################################################
  def disconnect(self):
    """Disables deduplication on this VDO device."""
    self._handlePreviousOperationFailure()

    try:
      version = VDOKernelModuleService().targetVersion()
      # comparison on each element
      if version > (6,2,0):
        runCommand(["dmsetup", "message", self.getName(), "0", "index-close"])
      else:
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
      raise ServiceError(msg, exitStatus = StateExitStatus)

    newLogicalSize.roundToBlock()
    if newLogicalSize < self.logicalSize:
      msg = _("Can't shrink a VDO volume (old size {0})").format(
              self.logicalSize)
      raise ServiceError(msg, exitStatus = UserExitStatus)
    elif newLogicalSize == self.logicalSize:
      msg = _("Can't grow a VDO volume by less than {0} bytes").format(
              Constants.VDO_BLOCK_SIZE)
      raise ServiceError(msg, exitStatus = UserExitStatus)

    # Do the grow.
    self._setOperationState(self.OperationState.beginGrowLogical)

    self.log.info(_("Increasing logical size of VDO volume {0}").format(
      self.getName()))

    numSectors = newLogicalSize.toSectors()
    vdoConf = self._generateModifiedDmTable(numSectors = str(numSectors))

    transaction = Transaction.transaction()
    transaction.setMessage(self.log.error,
                           _("Device {0} could not be changed").format(
                              self.getName()))
    runCommand(["dmsetup", "reload", self._name, "--table", vdoConf])
    transaction.setMessage(None)
        
    self._suspend(False)
    self._resume()

    # Get the new logical size
    vdoConfig = self._getVDOConfigFromVDO()
    logicalSize = (vdoConfig["logicalBlocks"]
                   * (vdoConfig["blockSize"] // Constants.SECTOR_SIZE))
    self.logicalSize = SizeString("{0}s".format(logicalSize))

    self.log.info(_("Increased logical size of VDO volume {0}").format(
      self.getName()))

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
      raise ServiceError(msg, exitStatus = StateExitStatus)

    newPhysicalSize = self._getDeviceSize(self.device)
    newPhysicalSize.roundToBlock()

    if newPhysicalSize < self.physicalSize:
      msg = _("Can't shrink a VDO volume (old size {0})").format(
              self.physicalSize)
      raise ServiceError(msg, exitStatus = UserExitStatus)
    elif newPhysicalSize == self.physicalSize:
      msg = _("Can't grow a VDO volume by less than {0} bytes").format(
              Constants.VDO_BLOCK_SIZE)
      raise ServiceError(msg, exitStatus = UserExitStatus)

    # Do the grow.
    self._setOperationState(self.OperationState.beginGrowPhysical)

    self.log.info(_("Increasing physical size of VDO volume {0}").format(
      self.getName()))

    vdoConf = self._generateModifiedDmTable(storageSize
                                            = str(newPhysicalSize.toBlocks()))

    transaction = Transaction.transaction()
    transaction.setMessage(self.log.error,
                           _("Device {0} could not be changed").format(
                              self.getName()))
    runCommand(["dmsetup", "reload", self._name, "--table", vdoConf])
    transaction.setMessage(None)
    
    self._suspend(False)
    self._resume()

    # Get the new physical size
    vdoConfig = self._getVDOConfigFromVDO()
    sectorsPerBlock = vdoConfig["blockSize"] // Constants.SECTOR_SIZE
    physicalSize = vdoConfig["physicalBlocks"] * sectorsPerBlock
    self.physicalSize = SizeString("{0}s".format(physicalSize))

    self.log.info(_("Increased physical size of VDO volume {0}").format(
      self.getName()))

    # The grow is done.
    self._setOperationState(self.OperationState.finished)

  ######################################################################
  @transactional
  def importDevice(self):
    """Imports and starts a VDO target."""
    self._announce(_("Importing VDO {0}").format(self.getName()))
    self.log.debug("confFile is {0}".format(self.config.filepath))

    self._handlePreviousOperationFailure()

    # Import creation parameters from the disk, making sure we don't
    # overwrite uuid if its passed in.
    config = self._getConfigFromVDO()
    vdoConfig = config["VDOConfig"]
    sectorsPerBlock = vdoConfig["blockSize"] // Constants.SECTOR_SIZE
    physicalSize = vdoConfig["physicalBlocks"] * sectorsPerBlock
    self.physicalSize = SizeString("{0}s".format(physicalSize))
    logicalSize = vdoConfig["logicalBlocks"] * sectorsPerBlock
    self.logicalSize = SizeString("{0}s".format(logicalSize))
    self.slabSize = vdoConfig["slabSize"]
    
 
    indexConfig = config["IndexConfig"] 
    self.indexMem = indexConfig["memory"]
    self.indexSparse = indexConfig["sparse"]
    self.indexCfreq = indexConfig["checkpointFrequency"]

    uuid = self.uuid
    self.uuid = config["UUID"] 
    if uuid is not None:
      self.uuid = uuid

    # Check for various creation issues.
    self._checkForExistingVDO()

    # Validate some parameters.
    self._validateParameters()

    # Check for uuid conflicts among all vdos we can find.
    self._checkForExistingUUID(self.uuid)    

    # Find a stable name for the storage device.
    self._setStableName()

    # Make certain the kernel module is installed.
    self._installKernelModule(self.vdoLogLevel)

    # Do the import.
    self._setOperationState(self.OperationState.beginImport)

    # As setting the operation state updates (and persists) the config file
    # we need to be certain to remove this instance if something goes wrong.
    transaction = Transaction.transaction()
    transaction.addUndoStage(self.config.persist)
    transaction.addUndoStage(functools.partial(self.config.removeVdo,
                                               self.getName()))
    self._setUUID(self.uuid)

    self._constructServiceStart()

    # The create is done.
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
    self._announce(_("Removing VDO {0}").format(self.getName()))

    # Fail if the device does not exist and --force is not specified. If
    # this remove is being run to undo a failed create, the device will
    # exist.
    try:
      os.stat(self.device)
    except OSError:
      if not force:
        msg = _("Device {0} not found. Remove VDO with --force.").format(
          self.device)
        raise VDOMissingDeviceError(msg)

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
    #
    # Having gotten this far we know that either no one is holding on to the
    # underlying device or we skipped that check because we weren't running.
    # We could be in one of a number of clean up conditions and only if the
    # underlying device isn't in use can we clear the metadata.
    #
    # This really isn't sufficient but we cannot completely determine that no
    # one has data of any import on the device.  For example, if the device was
    # being used raw we have no way of determining that.  We do what we can.
    if not self._hasHolders():
      self._clearMetadata()

  ######################################################################
  def running(self):
    """Returns True if the VDO service is available."""
    try:
      result = runCommand(["dmsetup", "status", "--target",
                           Defaults.vdoTargetName, self.getName()])
      # dmsetup does not error as long as the device exists even if it's not
      # of the specified target type.  However, if it's not of the specified
      # target type there is no returned info.
      return result.strip() != ""
    except Exception:
      return False

  ######################################################################
  def start(self, forceRebuild=False):
    """Starts the VDO target mapper. In noRun mode, we always assume
    the service is not yet running.

    Raises:
      ServiceError
    """
    self._announce(_("Starting VDO {0}").format(self.getName()))

    self._handlePreviousOperationFailure()

    if not self.activated:
      self.log.info(_("VDO service {0} not activated").format(self.getName()))
      return
    if self.running() and not Command.noRunMode():
      self.log.warning(
        _("VDO service {0} already started; no changes made").format(
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
      self._startFullnessMonitoring()
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
    status[self.vdoCompressionEnabledKey] = Constants.enableString(
                                              self.enableCompression)
    status[self.vdoDeduplicationEnabledKey] = Constants.enableString(
                                                self.enableDeduplication)
    status[self.vdoLogicalSizeKey] = str(self.logicalSize)
    status[self.vdoMaxDiscardSizeKey] = str(self.maxDiscardSize)
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
    status[_("UUID")] = self._getUUID()
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
    self._announce(_("Stopping VDO {0}").format(self.getName()))

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
        self.log.warning(_("VDO service {0} already stopped").format(
            self.getName()))
        return

    running = self.running()
    if running and self._hasHolders():
      msg = _("cannot stop VDO volume {0}: in use").format(self.getName())
      raise ServiceError(msg, exitStatus = StateExitStatus)

    if (running and self._hasMounts()) or (not execute):
      command = ["umount", "-f", self.getPath()]
      if removeSteps is not None:
        removeSteps.append(" ".join(command))

      if execute:
        if force:
          runCommand(command, noThrow=True)
        else:
          msg = _("cannot stop VDO volume with mounts {0}").format(
                  self.getName())
          raise ServiceError(msg, exitStatus = StateExitStatus)

    # The udevd daemon can wake up at any time and use the blkid command on our
    # vdo device.  In fact, it can be triggered to do so by the unmount command
    # we might have just done.  Wait for udevd to process its event queue.
    command = ["udevadm", "settle"]
    if removeSteps is not None:
      removeSteps.append(" ".join(command))
    if running and execute:
      runCommand(command, noThrow=True)

    if running:
      self._stopFullnessMonitoring(execute, removeSteps)

    # In a modern Linux, we would use "dmsetup remove --retry".
    # But SQUEEZE does not have the --retry option.
    command = ["dmsetup", "remove", self.getName()]
    if removeSteps is not None:
      removeSteps.append(" ".join(command))

    inUse = True
    if running and execute:
      for unused_i in range(10):
        try:
          runCommand(command)
          return
        except Exception as ex:
          if "Device or resource busy" not in str(ex):
            inUse = False
            break
        time.sleep(1)

    # If we're not executing we're in a previous operation failure situation
    # and want to report that and go no further.
    if not execute:
      self._generatePreviousOperationFailureResponse()

    # Because we may removed the instance above we have to check again to see
    # if it's running rather than using the value we got above.
    if self.running():
      if inUse:
        msg = _("cannot stop VDO service {0}: device in use").format(
          self.getName())
      else:
        msg = _("cannot stop VDO service {0}").format(self.getName())
      raise ServiceError(msg, exitStatus = SystemExitStatus)

  ######################################################################
  def setCompression(self, enable):
    """Changes the compression setting on a VDO.  If the VDO is running
    the setting takes effect immediately.
    """
    if enable:
      self._announce("Enabling compression on vdo {name}".format(
        name=self.getName()))
    else:
      self._announce("Disabling compression on vdo {name}".format(
        name=self.getName()))

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
    if enable:
      self._announce("Enabling deduplication on vdo {name}".format(
        name=self.getName()))
    else:
      self._announce("Disabling deduplication on vdo {name}".format(
        name=self.getName()))
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
                             .format(self.getName()),
                             exitStatus = SystemExitStatus)
        elif status == Constants.deduplicationStatusOpening:
          message = (_("Timeout enabling deduplication for {0}, continuing")
                     .format(self.getName()))
          self.log.warn(message)
        else:
          message = (_("Unexpected kernel status {0} enabling deduplication for {0}")
                     .format(status, self.getName()))
          raise ServiceError(message, exitStatus = SystemExitStatus)
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
        if option == "uuid":
          if self.running():
            self._announce("Can't modify uuid on VDO {0} while it is running."
                           " Skipping change.".format(self.getName()))
            continue
          try:
            self._checkForExistingUUID(value)
            self._setUUID(value)
          except VDOServiceError as ex:
            self._announce("Can't modify uuid on VDO {0}. Skipping change: {1}."
                           .format(self.getName(), ex))
            continue          
          setattr(self, self.modifiableOptions[option], value)
          modified = True
        else:
          setattr(self, self.modifiableOptions[option], value)
          modified = True

    if modified:
      self.config.addVdo(self.getName(), self, True)

      if self.running():
        self._announce(
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
            "maxDiscardSize",
            "_operationState",
            "physicalSize",
            "physicalThreads",
            "slabSize",
            "uuid",
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
    data["maxDiscardSize"] = str(self.maxDiscardSize)
    data["physicalSize"] = str(self.physicalSize)
    data["slabSize"] = str(self.slabSize)
    data["writePolicy"] = self.writePolicy
    return data

  ######################################################################
  def _yamlSetAttributes(self, attributes):
    super(VDOService, self)._yamlSetAttributes(attributes)
    # If an expected attribute does not exist in the specified dictionary the
    # current value is used.  This requires that the attribute be given an
    # appropriate default when the object is instantiated.
    self.activated = (
      self._defaultIfNone(attributes, "activated",
                          Constants.enableString(self.activated))
        != Constants.disabled)

    self.blockMapCacheSize = SizeString(
      self._defaultIfNone(attributes, "blockMapCacheSize",
                          str(self.blockMapCacheSize)))

    self.enableCompression = (
      self._defaultIfNone(attributes, "compression",
                          Constants.enableString(self.enableCompression))
        != Constants.disabled)

    self.enableDeduplication = (
      self._defaultIfNone(attributes, "deduplication",
                          Constants.enableString(self.enableDeduplication))
        != Constants.disabled)

    self.indexSparse = (
      self._defaultIfNone(attributes, "indexSparse",
                          Constants.enableString(self.indexSparse))
        != Constants.disabled)

    self.logicalSize = SizeString(
      self._defaultIfNone(attributes, "logicalSize",
                          str(self.logicalSize)))

    self.maxDiscardSize = SizeString(
      self._defaultIfNone(attributes, "maxDiscardSize",
                          str(self.maxDiscardSize)))

    self.physicalSize = SizeString(
      self._defaultIfNone(attributes, "physicalSize",
                          str(self.physicalSize)))

    self.slabSize = SizeString(
      self._defaultIfNone(attributes, "slabSize",
                          str(self.slabSize)))

    # writePolicy is handled differently as it is a computed property which
    # depends on the config being set which is not the case when the instance
    # is instantiated from YAML.
    if "writePolicy" in attributes:
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
                     "maxDiscardSize",
                     "physicalSize",
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
    self.maxDiscardSize = self._defaultIfNone(kw, 'maxDiscardSize',
                                              Defaults.maxDiscardSize)
    self.mdRaid5Mode = Defaults.mdRaid5Mode
    self.physicalSize = SizeString("0")
    self.physicalThreads = self._defaultIfNone(kw, 'vdoPhysicalThreads',
                                               Defaults.physicalThreads)
    self.slabSize = self._defaultIfNone(kw, 'vdoSlabSize', Defaults.slabSize)
    self._writePolicy = self._defaultIfNone(kw, 'writePolicy',
                                            Defaults.writePolicy)
    self._writePolicySet = False  # track if the policy is explicitly set

    self.instanceNumber = 0

    self.vdoLogLevel = kw.get('vdoLogLevel')

    self.uuid = kw.get('uuid')
    
    self.indexCfreq = self._defaultIfNone(kw, 'cfreq', Defaults.cfreq)
    self._setMemoryAttr(self._defaultIfNone(kw, 'indexMem',
                                            Defaults.indexMem))
    sparse = self._defaultIfNone(kw, 'sparseIndex', Defaults.sparseIndex)
    self.indexSparse = (sparse != Constants.disabled)
    self.indexThreads = self._defaultIfNone(kw, 'udsParallelFactor',
                                            Defaults.udsParallelFactor)

  ######################################################################
  def __setattr__(self, name, value):
    if name == 'indexMemory':
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

    # We need to round all logical and physical sizes.
    if name in ['maxDiscardSize', 'logicalSize', 'physicalSize']:
      getattr(self, name).roundToBlock()

  ######################################################################
  # Protected methods
  ######################################################################
  @staticmethod
  def _defaultIfNone(args, name, default):
    value = args.get(name)
    return default if value is None else value

  ######################################################################
  def _announce(self, message):
    self.log.info(message)
    print(message)

  ######################################################################
  def _checkConfiguration(self):
    """Check and fix the configuration of this VDO.

    Raises:
      ServiceError
    """
    cachePages = self.blockMapCacheSize.toBlocks()
    if cachePages < 2 * 2048 * self.logicalThreads:
      msg = _("Insufficient block map cache for {0}").format(self.getName())
      raise ServiceError(msg, exitStatus = UserExitStatus)

    # Adjust the block map period to be in its acceptable range.
    self.blockMapPeriod = max(self.blockMapPeriod, Defaults.blockMapPeriodMin)
    self.blockMapPeriod = min(self.blockMapPeriod, Defaults.blockMapPeriodMax)

  ######################################################################
  def _checkForExistingUUID(self, uuid):
    """ Checks that the device does not have an already existing UUID.

    Arguments:
      uuid (str) - the uuid to check for

    Raises: VDOServiceError
    """
    # If we're planning on generating a new random uuid there is no
    # need to check for existing uuid as we assume random is random
    # enough.
    if uuid is None or uuid == "":
      return

    # get unique set of known running vdo storage devices.
    cmd = ['dmsetup', 'table', '--target', Defaults.vdoTargetName]
    vdos = set([line.split(' ')[5]
                for line in runCommand(cmd, noThrow=True).splitlines()])
    # add to it a list of known offline vdo storage devices.
    vdos |= set([vdo.device for vdo in self.config.getAllVdos().values()])
    vdos = list(vdos)

    conflictVdos = []
    for vdo in vdos:
      try:
        config = yaml.safe_load(runCommand(["vdodumpconfig", vdo]))
        if uuid == config["UUID"]:
          conflictVdos.append(vdo) 
      except:
        pass
    if len(conflictVdos) > 0:
      conflictList = ", ".join(conflictVdos)
      msg = _("UUID {0} already exists in VDO volume(s) stored on {1}").format(
        uuid, conflictList)
      raise VDOServiceError(msg, exitStatus = StateExitStatus)
      
  ######################################################################
  def _checkForExistingVDO(self):
    """Check to see if there is an existing VDO volume running with the 
       same name as this VDO, or it already exists in this config file

    Raises:
      VDOServiceExistsError VDODeviceAlreadyConfiguredError
    """
    if self.isConstructed:
      msg = _("VDO volume {0} already exists").format(self.getName())
      raise VDOServiceExistsError(msg)

    # Check that there isn't already a vdo using the device we were given.
    if self.config.isDeviceConfigured(self.device):
      msg = _("Device {0} already configured for VDO use").format(
              self.device)
      raise VDODeviceAlreadyConfiguredError(msg)

    # Check that there isn't an already extant dm target with the VDO's name.
    if self._mapperDeviceExists():
      msg = _("Name conflict with extant device mapper target {0}").format(
              self.getName())
      raise VDOServiceError(msg, exitStatus = StateExitStatus)

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
            and (
              self.operationState in [self.OperationState.beginCreate,
                                      self.OperationState.beginImport]
            ))

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

    vdoConfig = self._getVDOConfigFromVDO()
    sectorsPerBlock = vdoConfig["blockSize"] // Constants.SECTOR_SIZE
    physicalSize = vdoConfig["physicalBlocks"] * sectorsPerBlock
    self.physicalSize = SizeString("{0}s".format(physicalSize))
    logicalSize = vdoConfig["logicalBlocks"] * sectorsPerBlock
    self.logicalSize = SizeString("{0}s".format(logicalSize))

  ######################################################################
  def _createCheckCleanDevice(self):
    """Performs a verification for create that the storage device doesn't
    already have something on it.

    Raises:
      VDOServiceError
    """
    # Perform the same checks that LVM does (which doesn't yet include checking
    # for an already-formatted VDO volume, but vdoformat does that), so we do
    # it by...actually making LVM do it for us!
    try:
      runCommand(['pvcreate', '--config', 'devices/scan_lvs=1',
                  '-qq', '--test', self.device])
    except CommandError as e:
      # Messages from pvcreate aren't localized, so we can look at
      # the message generated and pick it apart. This will need
      # fixing if the message format changes or it gets localized.
      lines = [line.strip() for line in e.getStandardError().splitlines()]
      lineCount = len(lines)
      if lineCount > 0:
        for i in range(lineCount):       
          if (re.match(r"^TEST MODE", lines[i]) is not None):
            for line in lines[i+1:]:
              detectionMatch = re.match(r"WARNING: (.* detected .*)"
                                        "\.\s+Wipe it\?", line)
              if detectionMatch is not None:
                raise VDOServiceError('{0}; use --force to override'
                                      .format(detectionMatch.group(1)),
                                      exitStatus = StateExitStatus)
            break
        # Use the last line from the test output.
        # This will be the human-useful description of the problem.
        e.setMessage(lines[-1])
      # No TEST MODE message, just keep going.
      raise e

    # If this is a physical volume that's not in use by any logical
    # volumes the above check won't trigger an error. So do a second
    # check that catches that.
    try:
      runCommand(['blkid', '-p', self.device])
    except CommandError as e:
      if e.getExitCode() == 2:
        return
    raise VDOServiceError('device is a physical volume;'
                          + ' use --force to override',
                          exitStatus = StateExitStatus)

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
    maxDiscardBlocks = self.maxDiscardSize.toBlocks()
    threadCountConfig = " ".join(["ack", str(self.ackThreads),
                                  "bio", str(self.bioThreads),
                                  "bioRotationInterval",
                                   str(self.bioRotationInterval),
                                  "cpu", str(self.cpuThreads),
                                  "hash", str(self.hashZoneThreads),
                                  "logical", str(self.logicalThreads),
                                  "physical", str(self.physicalThreads)])
    vdoConf = " ".join(["0", str(numSectors), Defaults.vdoTargetName,
                        "V2", self.device,
                        str(self._getVDOConfigFromVDO()['physicalBlocks']),
                        str(self.logicalBlockSize),
                        str(cachePages), str(self.blockMapPeriod),
                        self.mdRaid5Mode, self.writePolicy,
                        self._name,
                        "maxDiscard", str(maxDiscardBlocks),
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
    tableOrder = ("logicalStart numSectors targetName version storagePath"
                  + " storageSize blockSize cacheBlocks blockMapPeriod"
                  + " mdRaid5Mode writePolicy poolName")

    tableOrderItems = tableOrder.split(" ")
    tableItems = table.split(" ")

    # This will set up only required parameters due to length of tableOrder
    dmTable = dict(zip(tableOrderItems, tableItems))

    # Apply new values
    for (key, val) in list(kwargs.items()):
      dmTable[key] = val

    # Create and return the new table
    return " ".join([dmTable[key] for key in tableOrderItems]
                    + tableItems[len(dmTable):])

  ######################################################################
  def _generatePreviousOperationFailureResponse(self):
    """Generates the required response to a previous operation failure.

    Logs a message indicating that the previous operation failed and raises the
    VDOServicePreviousOperationError exception with the same message.

    Arguments:
      operation (str) - the operation that failed; default to "create".

    Raises:
      VDOServicePreviousOperationError
    """

    msg = _("VDO volume {0} previous operation ({1}) is incomplete{2}").format(
            self.getName(), self.OperationState.names[self.operationState],
            "; recover by performing 'remove --force'")
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
      raise VDOServiceError(msg, exitStatus = DeveloperExitStatus)
    elif self.operationState == self.OperationState.beginCreate:
      # Create is not automatically recovered.
      self._generatePreviousOperationFailureResponse()
    elif self.operationState == self.OperationState.beginGrowLogical:
      self._recoverGrowLogical()
    elif self.operationState == self.OperationState.beginGrowPhysical:
      self._recoverGrowPhysical()
    elif self.operationState == self.OperationState.beginImport:
      # Import is not automatically recovered.
      self._generatePreviousOperationFailureResponse()
    elif self.operationState == self.OperationState.beginRunningSetWritePolicy:
      self._recoverRunningSetWritePolicy()
    else:
      msg = _("Missing handler for recover from operation state: {0}").format(
              self.operationState)
      raise VDOServiceError(msg, exitStatus = DeveloperExitStatus)

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
    return config

  ######################################################################
  def _getIndexConfigFromVDO(self):
    """Returns a dictionary of the index configuration values as reported
    by the actual uds index used by the vdo.
    """
    config = yaml.safe_load(runCommand(["vdodumpconfig",
                                        self.device]))
    return config["IndexConfig"]

  ######################################################################
  def _getUUID(self):
    """Returns the uuid as reported from the actual vdo storage.
    """
    config = yaml.safe_load(runCommand(["vdodumpconfig",
                                        self.device]))
    return "VDO-" + config["UUID"]

  ######################################################################
  def _getVDOConfigFromVDO(self):
    """Returns a dictionary of the configuration values as reported from
    the actual vdo storage.
    """
    config = yaml.safe_load(runCommand(["vdodumpconfig",
                                        self.device]))
    return config["VDOConfig"]

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
  def _getDeviceSize(self, devicePath):
    """Get the size of the device passed in.

    Arguments:
      devicePath (path): path to a device.

    Returns:
      Size of device as a SizeString object
    """
    basePath = self._getBaseDevice(devicePath);
    baseName = os.path.basename(basePath)
    output = runCommand(["cat", "/sys/class/block/" + baseName + "/size"]);
    return SizeString("{0}s".format(output));

  ######################################################################
  def _getDeviceUUID(self, devicePath):
    """Get the UUID of the device passed in,

    Arguments:
      devicePath (path): path to a device.

    Returns:
      UUID as a string, or None if none found
    """
    try:
      output = runCommand(["blkid", "-s", "UUID", "-o", "value",
                           devicePath]).strip()
    except CommandError as ex:
      self.log.info("blkid failed: " + str(ex))
      return None
    if output == "":
      return None
    else:
      return output

  ######################################################################
  def _hasHolders(self):
    """Tests whether other devices are holding the VDO device open. This
    handles the case where there are LVM entities stacked on top of us.

    Returns:
      True iff the VDO device has something holding it open.
    """
    try:
      st = os.stat(self.getPath())
      major = os.major(st.st_rdev)
      minor = os.minor(st.st_rdev)
    except OSError:
      return False

    holdersDirectory = "/sys/dev/block/{major}:{minor}/holders".format(
      major=major, minor=minor)

    if os.path.isdir(holdersDirectory):
      holders = os.listdir(holdersDirectory)
      if len(holders) > 0:
        self.log.info("{path} is being held open by {holders}".format(
          path=self.getPath(), holders=" ".join(holders)))
        return True
    return False

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
  def _mapperDeviceExists(self):
    """Returns True if there already exists a dm target with the name of
    this VDO."""
    try:
      os.stat(self.getPath())
      return True
    except OSError:
      return False

  ######################################################################
  @transactional
  def _performRunningSetWritePolicy(self):
    """Peforms the changing of the write policy on a running vdo instance.
    """
    transaction = Transaction.transaction()
    transaction.setMessage(self.log.error,
                           _("Device {0} could not be read").format(
                                                          self.getName()))
    vdoConf = self._generateModifiedDmTable(writePolicy = self.writePolicy)

    transaction.setMessage(self.log.error,
                           _("Device {0} could not be changed").format(
                                                          self.getName()))
    runCommand(["dmsetup", "reload", self._name, "--table", vdoConf])
    transaction.setMessage(None)
    
    self._suspend(False)
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
      raise VDOServiceError(msg, exitStatus = DeveloperExitStatus)
    else:
      # Get the correct logical size from vdo.
      vdoConfig = self._getVDOConfigFromVDO()
      logicalSize = (vdoConfig["logicalBlocks"]
                      * (vdoConfig["blockSize"] // Constants.SECTOR_SIZE))
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
      raise VDOServiceError(msg, exitStatus = DeveloperExitStatus)
    else:
      # Get the correct physical size from vdo.
      vdoConfig = self._getVDOConfigFromVDO()
      physicalSize = (vdoConfig["physicalBlocks"]
                      * (vdoConfig["blockSize"] // Constants.SECTOR_SIZE))
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
      raise VDOServiceError(msg, exitStatus = DeveloperExitStatus)
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
    self._startFullnessMonitoring()
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

  #####################################################################
  def _setStableName(self):
    # Find a stable name for the real device, that won't change from
    # one boot cycle to the next.
    realpath = os.path.realpath(self.device)
    idDir = '/dev/disk/by-id'
    aliases = []
    if os.path.isdir(idDir):
      aliases = [absname
                 for absname in (os.path.join(idDir, name)
                                 for name in os.listdir(idDir))
                 if os.path.realpath(absname) == realpath]
      if realpath is not None:
        deviceUUID = self._getDeviceUUID(realpath)
        if deviceUUID is not None:
          self.log.debug("pruning {uuid} from aliases".format(
            uuid=deviceUUID))
          aliases = [a for a in aliases if not deviceUUID in a]
      
    if len(aliases) > 0:
      self.log.debug("found aliases for {original}: {aliases}"
                     .format(original = realpath, aliases = aliases))

      # A device can have multiple names; dm-name-*, dm-uuid-*, ata-*,
      # wwn-*, etc.  Do we have a way to prioritize them?
      #
      # LVM volumes and MD arrays can be renamed; prioritize dm-name-*
      # below dm-uuid-* and likewise for md-*.
      #
      # Otherwise, just sort and take the first name; that'll at least
      # be consistent from run to run.
      uuidAliases = [a
                     for a in aliases
                     if re.match(r".*/[dm][dm]-uuid-", a) is not None]
      if len(uuidAliases) > 0:
        aliases = uuidAliases
      aliases.sort()
      self.device = aliases[0]
      self.log.debug("using {new}".format(new = self.device))
    else:
      self.log.debug("no aliases for {original} found in {idDir}!"
                     .format(original = realpath, idDir = idDir))
      
  ######################################################################
  def _setUUID(self, uuid):
    """Sets a new uuid for the vdo volume, either by providing a value
    or having the tool generate a new random one.

    Arguments:
      uuid (str) - the uuid to set

    Raises:
      Exception
    """
    if uuid is None:
      return
    
    try:
      if uuid == "":
        runCommand(["vdosetuuid", self.device])
      else:
        runCommand(["vdosetuuid", "--uuid", uuid, self.device])        
    except Exception as ex:
      msg = _("Can't set the UUID for VDO volume {0}; {1!s}").format(
        self.getName(), ex)
      raise VDOServiceError(msg, exitStatus = StateExitStatus)

  ######################################################################
  def _startCompression(self):
    """Starts compression on a VDO volume if it is running.
    """
    self._toggleCompression(True)

  ######################################################################
  def _startFullnessMonitoring(self):
    try:
      runCommand(["vdodmeventd", "-r", self.getName()])
    except Exception:
      self.log.info(_("Could not register {0}"
                      " with dmeventd").format(self.getName()))
      pass

  ######################################################################
  def _stopCompression(self):
    """Stops compression on a VDO volume if it is running.
    """
    self._toggleCompression(False)

  ######################################################################
  def _stopFullnessMonitoring(self, execute, removeSteps):
    command = ["vdodmeventd", "-u", self.getName()]
    if removeSteps is not None:
      removeSteps.append(" ".join(command))
    if execute:
      runCommand(command, noThrow=True)

  ######################################################################
  def _suspend(self, flush=True):
    """Suspends a running VDO."""
    self.log.info(_("Suspending VDO volume {0} with {1}").format(
      self.getName(), "flush" if flush else "no flush"))
    self._stopFullnessMonitoring(True, None)
    try:
      runCommand(["dmsetup", "suspend", "" if flush else "--noflush",
                  self.getName()])
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

    self._announce(_("{0} compression on VDO {1}").format(
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
    # SizeString respects locale, so convert to localized representation
    memoryNeeded = SizeString("{0}g".format(locale.str(float(indexMemory))))
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

  ######################################################################
  def _validateParameters(self):    
    # Check that we have enough kernel memory to at least create the index.
    self._validateAvailableMemory(self.indexMemory);

    # Check that the hash zone, logical and physical threads are consistent.
    self._validateModifiableThreadCounts(self.hashZoneThreads,
                                         self.logicalThreads,
                                         self.physicalThreads)
    
