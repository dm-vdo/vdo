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
  VDOOperation - an object representing a vdo script command

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/VDOOperation.py#4 $
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from . import ArgumentError
from . import CommandLock
from . import Configuration
from . import Constants
from . import Defaults
from . import MgmntUtils
from . import Service
from . import VDOKernelModuleService
from . import VDOService, VDOServiceError, VDOServicePreviousOperationError
from . import ExitStatus, StateExitStatus, SystemExitStatus, UserExitStatus
from vdo.utils import Command, CommandError, runCommand
from vdo.utils import Transaction, transactional
from functools import partial
import inspect
import logging
import __main__ as main
import os
import re
import sys
import yaml

vdoOperations = dict()

def lock(isExclusive, func, *args, **kwargs):
  commandArgs = args[1]
  confFile = os.path.abspath(commandArgs.confFile)
  confFile = os.path.realpath(confFile)
  # N.B.: We don't filter out shell special characters like " (){}"!
  #
  # Also, no quoting; if two config file names use "/" and "_" such
  # that they both map to the same lock file, so be it.
  lockFileBase = confFile.replace('/', '_') + '.lock'
  lockFile = os.path.join(Constants.LOCK_DIR, lockFileBase)
  with CommandLock(lockFile, isExclusive):
    return func(*args, **kwargs)

def exclusivelock(func):
  "Decorator that locks the configuration for exclusive (write) access."
  def wrap(*args, **kwargs):
    return lock(False, func, *args, **kwargs)
  return wrap

def sharedlock(func):
  "Decorator that locks the configuration for shared (read) access."
  def wrap(*args, **kwargs):
    return lock(False, func, *args, **kwargs)
  return wrap

########################################################################
class OperationError(ExitStatus, Exception):
  """Exception raised to indicate an error executing an operation."""

  ######################################################################
  # Public methods
  ######################################################################

  ######################################################################
  # Overridden methods
  ######################################################################

  ######################################################################
  # Protected methods
  ######################################################################
  def __init__(self, msg, *args, **kwargs):
    super(OperationError, self).__init__(*args, **kwargs)
    self._msg = msg

  def __str__(self):
    return self._msg

########################################################################
class VDOOperation(object):
  """Every instance of this class runs one of the subcommands
  requested when 'vdo [<options>] <subcommand>' is called via the
  execute() method."""

  ######################################################################
  # Public methods
  ######################################################################
  def getVdoServices(self, args, conf):
    """Return a list of VDOService objects to be operated on depending
    on the settings of the --name and --all options.
    Arguments:
      args: The arguments passed into vdo.
      conf: The config file
    Raises:
      ArgumentError
    """
    services = []
    if args.all:
      services.extend(conf.getAllVdos().values())
    else:
      self._checkForName(args)
      services.append(conf.getVdo(args.name))
    return services

  ######################################################################
  def applyToVDOs(self, args, method, **kwargs):
    """Apply a method to all specified VDOs. An exception applying the method
    to some VDO will not prevent it from being applied to any other VDO,
    however any exception will result in applyToVDOs raising an exception.

    If the 'readonly' keyword argument is False, the configuration will be
    persisted after the method has been applied to all the VDOs (whether or
    not it succeeded for any of them).

    Arguments:
      args (dict):       The command line arguments
      method (callable): The method to call on each VDO; will be called as
                         method(args, vdo)
      kwargs:            Keyword args controlling what gets returned
                          and to use when making the Configuration
    """
    exception = None
    conf = None

    if kwargs.get('readonly', True):
      conf = Configuration(self.confFile, **kwargs)
    else:
      conf = Configuration.modifiableSingleton(self.confFile)

    for vdo in self.getVdoServices(args, conf):
      try:
        method(args, vdo)
      except Exception as ex:
        exception = ex
    if not kwargs.get('readonly', True):
      conf.persist()
    if exception is not None:
      #pylint: disable=E0702
      raise exception

  ######################################################################
  def execute(self, unused_args):
    """Execute this operation. This method should be overridden by operation
    classes, and is intended only to be called from within the run() method
    below.

    Arguments:
      unused_args (dict): The command line arguments (used by subclasses)
    Raises:
      NotImplementedError
    """
    self.log.error(_("{0} unimplemented").format(self.name))
    raise NotImplementedError

  ######################################################################
  def preflight(self, args):
    """Perform checks prior to actually executing the command.

    Arguments:
      args (dict): The command line arguments (used by subclasses)
    Raises:
      OperationError
    """
    if self.requiresRoot and (os.getuid() != 0):
      msg = _("You must be root to use the \"{0}\" command").format(self.name)
      raise OperationError(msg, exitStatus = UserExitStatus)

    if self.checkBinaries:
      for executable in ['vdodumpconfig',
                         'vdoforcerebuild',
                         'vdoformat']:
        if not MgmntUtils.which(executable):
          msg = _("executable '{0}' not found in $PATH").format(executable)
          raise OperationError(msg, exitStatus = SystemExitStatus)

    if self.requiresRunMode and Command.noRunMode():
      msg = _("{0} command not available with --noRun").format(self.name)
      raise OperationError(msg, exitStatus = UserExitStatus)

  ######################################################################
  def run(self, args):
    """Run this operation. This is the external entry point for users of
    VDOOperation.

    Arguments:
      args (dict): The command line arguments
    """

    self.confFile = Defaults.confFile

    try:
      self.confFile = args.confFile
    except KeyError:
      pass

    self.preflight(args)
    self.execute(args)

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, **kwargs):
    name = re.sub('Operation$', '', type(self).__name__)
    self.name            = name[0].lower() + name[1:]
    self.log             = logging.getLogger('vdo.vdomgmnt.VDOOperation')
    self.requiresRoot    = kwargs.get('requiresRoot', True)
    self.checkBinaries   = kwargs.get('checkBinaries', False)
    self.requiresRunMode = kwargs.get('requiresRunMode', False)
    vdoOperations[self.name] = self

  ######################################################################
  # Protected methods
  ######################################################################
  def _checkForName(self, args):
    """Check that the args contain a non-None name.
    Arguments:
      args: the args passed
    Raises:
      ArgumentError
    """
    if (args.name is None) or (args.name.strip() == ""):
      raise ArgumentError(_("Missing required argument '--name'"))

########################################################################
class ActivateOperation(VDOOperation):
  """Implements the activate command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(ActivateOperation, self).__init__()

  ######################################################################
  @exclusivelock
  def execute(self, args):
    self.applyToVDOs(args, self._activate, readonly=False)

  ######################################################################
  # Protected methods
  ######################################################################
  def _activate(self, args, vdo):
    vdo.activate()

########################################################################
class ChangeWritePolicyOperation(VDOOperation):
  """Implements the changeWritePolicy command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(ChangeWritePolicyOperation, self).__init__()

  ######################################################################
  @exclusivelock
  def execute(self, args):
    self._newWritePolicy = getattr(args, 'writePolicy')
    if self._newWritePolicy is None:
      return
    self.applyToVDOs(args, self._changeWritePolicy, readonly=False)

  ######################################################################
  # Protected methods
  ######################################################################
  def _changeWritePolicy(self, args, vdo):
    vdo.setWritePolicy(self._newWritePolicy)

########################################################################
class CreateOperation(VDOOperation):
  """Implements the create command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(CreateOperation, self).__init__(checkBinaries=True)

  ######################################################################
  def preflight(self, args):
    super(CreateOperation, self).preflight(args)

    if not args.name:
      raise ArgumentError(_("Missing required argument '--name'"))

    if not args.device:
      raise ArgumentError(_("Missing required argument '--device'"))

  ######################################################################
  @exclusivelock
  @transactional
  def execute(self, args):
    # Get configuration
    conf = Configuration.modifiableSingleton(self.confFile)

    argsDict = vars(args).copy()
    name = argsDict['name']
    del argsDict['name']

    vdo = VDOService(args.name, conf, **argsDict)

    transaction = Transaction.transaction()
    vdo.create(args.force)
    transaction.addUndoStage(vdo.remove)

    conf.persist()
    vdo.announceReady()

########################################################################
class DeactivateOperation(VDOOperation):
  """Implements the deactivate command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(DeactivateOperation, self).__init__()

  ######################################################################
  @exclusivelock
  def execute(self, args):
    self.applyToVDOs(args, self._deactivate, readonly=False)

  ######################################################################
  # Protected methods
  ######################################################################
  def _deactivate(self, args, vdo):
    vdo.deactivate()

########################################################################
class GrowLogicalOperation(VDOOperation):
  """Implements the growLogical command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(GrowLogicalOperation, self).__init__()

  ######################################################################
  @exclusivelock
  def execute(self, args):
    conf = Configuration.modifiableSingleton(self.confFile)
    self._checkForName(args)

    vdo = conf.getVdo(args.name)
    vdo.growLogical(args.vdoLogicalSize)
    conf.persist()

########################################################################
class GrowPhysicalOperation(VDOOperation):
  """Implements the growPhysical command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(GrowPhysicalOperation, self).__init__()

  ######################################################################
  @exclusivelock
  def execute(self, args):
    conf = Configuration.modifiableSingleton(self.confFile)
    self._checkForName(args)

    vdo = conf.getVdo(args.name)
    vdo.growPhysical()
    conf.persist()

########################################################################
class ListOperation(VDOOperation):
  """Implements the list command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(ListOperation, self).__init__()

  ######################################################################
  def execute(self, args):
    vdos = set()
    for line in runCommand(['dmsetup', 'status'], noThrow=True).splitlines():
      m = re.match(r"(.+?): \d \d+ " + Defaults.vdoTargetName, line)
      if m:
        vdos.add(m.group(1))

    if args.all:
      conf = Configuration(self.confFile)
      vdos |= set(conf.getAllVdos().keys())

    # We want to provide a stable ordering and a set, while great for
    # avoiding duplicates, doesn't guarantee ordering.  So, make a list
    # from the set and sort it.
    vdos = list(vdos)
    vdos.sort()
    print(os.linesep.join(vdos))

########################################################################
class ModifyOperation(VDOOperation):
  """Implements the modify command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(ModifyOperation, self).__init__()

  ######################################################################
  @exclusivelock
  def execute(self, args):
    self.applyToVDOs(args, self._modifyVDO, readonly=False)

  ######################################################################
  def preflight(self, args):
    super(ModifyOperation, self).preflight(args)

    # Validate that the user didn't specify anything that can't be changed.
    VDOService.validateModifiableOptions(args)

  ######################################################################
  # Protected methods
  ######################################################################
  def _modifyVDO(self, args, vdo):
    vdo.setModifiableOptions(args)

########################################################################
class PrintConfigFileOperation(VDOOperation):
  """Implements the printConfigFile command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(PrintConfigFileOperation, self).__init__(requiresRoot=False,
                                                   requiresRunMode=True)

  ######################################################################
  @sharedlock
  def execute(self, args):
    #pylint: disable=R0201
    conf = Configuration(self.confFile, mustExist=True)
    print(conf.asYAMLForUser())

########################################################################
class RemoveOperation(VDOOperation):
  """Implements the remove command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(RemoveOperation, self).__init__()

  ######################################################################
  @exclusivelock
  def execute(self, args):
    """Implements the remove command."""
    self.applyToVDOs(args, self._removeVDO, readonly=False)

  ######################################################################
  # Protected methods
  ######################################################################
  def _removeVDO(self, args, vdo):
    removeSteps = []
    try:
      vdo.remove(args.force, removeSteps = removeSteps)
    except VDOServicePreviousOperationError:
      print(_("A previous operation failed."))
      print(_("Recovery from the failure either failed or was interrupted."))
      print(_("Add '--force' to 'remove' to perform the following cleanup."))
      print(os.linesep.join(removeSteps))
      raise

########################################################################
class StartOperation(VDOOperation):
  """Implements the start command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(StartOperation, self).__init__(checkBinaries=True)

  ######################################################################
  @exclusivelock
  def execute(self, args):
    self.applyToVDOs(args, self._startVDO, readonly=False)

  ######################################################################
  # Protected methods
  ######################################################################
  @transactional
  def _startVDO(self, args, vdo):
    if not vdo.activated:
      msg = _("VDO volume {name} not activated").format(name=vdo.getName())
      raise OperationError(msg, exitStatus = StateExitStatus)
    vdo.start(args.forceRebuild)
    vdo.announceReady(False)

########################################################################
class StatusOperation(VDOOperation):
  """Implements the status command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(StatusOperation, self).__init__(requiresRoot=False,
                                          checkBinaries=True,
                                          requiresRunMode=True)

  ######################################################################
  @exclusivelock
  def execute(self, args):
    #pylint: disable=R0201
    conf = Configuration(self.confFile, readonly=False)
    if not args.name:
      args.all = True

    try:
      # To be consistent with previous output we must present each section as
      # its own rather than organizing them into one structure to dump.
      # Also, we gather all the info before printing it out to avoid
      # interspersing command info when run in verbose mode.
      values = {}
      vdoStatus = { _("VDO status") : values }
      values[_("Node")] = runCommand(['uname', '-n'], noThrow=True, strip=True)
      values[_("Date")] = runCommand(['date', '--rfc-3339=seconds'],
                                      noThrow=True, strip=True)
      if os.getuid() != 0:
        values[_("Note")] = _("Not running as root,"
                              + " some status may be unavailable")

      kernelStatus = { _("Kernel module") : VDOKernelModuleService().status() }

      confStatus = { _("Configuration") : conf.status() }

      vdos = {}
      perVdoStatus = { _("VDOs") : vdos }
      for vdo in self.getVdoServices(args, conf):
        try:
          vdos[vdo.getName()] = vdo.status()
        except VDOServiceError as ex:
          vdos[vdo.getName()] = str(ex)

      # YAML adds a newline at the end.  To maintain consistency with the
      # previous output we need to eliminate that.
      print(yaml.safe_dump(vdoStatus, default_flow_style = False)[:-1])
      print(yaml.safe_dump(kernelStatus, default_flow_style = False)[:-1])
      print(yaml.safe_dump(confStatus, default_flow_style = False)[:-1])
      print(yaml.safe_dump(perVdoStatus, default_flow_style = False, 
                           width=float("inf"))[:-1])

      sys.stdout.flush()
      sys.stderr.flush()
    except IOError as ex:
      self.log.debug("exception ignored: {0}".format(ex))

########################################################################
class StopOperation(VDOOperation):
  """Implements the stop command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(StopOperation, self).__init__()

  ######################################################################
  @exclusivelock
  def execute(self, args):
    self.applyToVDOs(args, self._stopVDO, readonly=False)

  ######################################################################
  # Protected methods
  ######################################################################
  def _stopVDO(self, args, vdo):
    vdo.stop(args.force)

########################################################################
class VersionOperation(VDOOperation):
  """Implements the version command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(VersionOperation, self).__init__()

  ######################################################################
  def execute(self, unused_args):
    kms = VDOKernelModuleService()
    kms.start()
    print(kms.version())

########################################################################
class OptionToggle(VDOOperation):
  """Base class for operations which either enable or disable an option."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, enable, optionName, checkBinaries=False):
    super(OptionToggle, self).__init__(checkBinaries=checkBinaries)
    self._enable     = enable
    self._optionName = optionName

  ######################################################################
  @exclusivelock
  def execute(self, args):
    self.applyToVDOs(args, self._configure, readonly=False)

  ######################################################################
  # Protected methods
  ######################################################################
  def _configure(self, args, vdo):
    """Actually update the configuration for this operation. This method must
    be overridden by derived classes."""
    self.log.error(_("{0} unimplemented").format(self.name))
    raise NotImplementedError

########################################################################
class DisableCompressionOperation(OptionToggle):
  """Implements the disableCompression command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(DisableCompressionOperation, self).__init__(False, "compression")

  ######################################################################
  def _configure(self, args, vdo):
    vdo.setCompression(False)

########################################################################
class DisableDeduplicationOperation(OptionToggle):
  """Implements the disable deduplication command."""

  ######################################################################
  # Protected methods
  ######################################################################
  def __init__(self):
    super(DisableDeduplicationOperation, self).__init__(False, "deduplication")

  ######################################################################
  def _configure(self, args, vdo):
    vdo.setDeduplication(False)

########################################################################
class EnableCompressionOperation(OptionToggle):
  """Implements the enableCompression command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(EnableCompressionOperation, self).__init__(True, "compression")

  ######################################################################
  def _configure(self, args, vdo):
    vdo.setCompression(True)

########################################################################
class EnableDeduplicationOperation(OptionToggle):
  """Implements the enableDeduplication command."""

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(EnableDeduplicationOperation, self).__init__(True, "deduplication",
                                                       checkBinaries=True)

  ######################################################################
  def _configure(self, args, vdo):
    vdo.setDeduplication(True)

########################################################################
operationRE = re.compile("^[A-Z].*Operation$")

########################################################################
def _isOperation(member):
  return (inspect.isclass(member) and (member.__name__ != "VDOOperation")
          and operationRE.match(member.__name__))

########################################################################
def makeOperations(moduleName):
  for operation in inspect.getmembers(sys.modules[moduleName], _isOperation):
    operation[1]()

########################################################################
makeOperations(__name__)
