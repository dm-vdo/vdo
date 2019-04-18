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
  Configuration - VDO manager configuration file handling

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/Configuration.py#6 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from . import ArgumentError
from . import StateExitStatus
from . import VDOService
from vdo.utils import Command, runCommand
from vdo.utils import FileLock, YAMLObject

import errno
import logging
import os
from stat import ST_MTIME
import time
import yaml

class BadConfigurationFileError(StateExitStatus, Exception):
  """Exception raised to indicate an error in processing the
  configuration file, such as a parse error or missing data.
  """

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, msg, *args, **kwargs):
    super(BadConfigurationFileError, self).__init__(*args, **kwargs)
    self._msg = msg

  ######################################################################
  def __str__(self):
    return self._msg

########################################################################
class Configuration(YAMLObject):
  "Configuration of VDO volumes."

  log = logging.getLogger('vdo.vdomgmnt.Configuration')
  supportedSchemaVersions = [0x20170907]
  modifiableSingltons = {}
  singletonLock = '/var/lock/vdo-config-singletons'
  yaml_tag = "!Configuration"
  yaml_has_unicode_ctors = False

  ######################################################################
  # Public methods
  ######################################################################
  @classmethod
  def modifiableSingleton(cls, filepath):
    """Allocates, as necessary, and returns a modifiable, singleton
    Configuration instance for the specified filepath.  Separate entities can
    thus share one in memory copy of the configuration file allowing for
    encapsulation of per-entity manipulation of the configuration.

    Args:
      filepath (str):   path to config file
    """
    config = None
    with FileLock(cls.singletonLock, "r+") as f:
      config = cls.modifiableSingltons.get(filepath)
      if config is None:
        config = Configuration(filepath, readonly = False)
        cls.modifiableSingltons[filepath] = config
    return config

  ######################################################################
  def addVdo(self, name, vdo, replace=False):
    """Adds or replaces a VDOService object in the configuration.
    Generates an assertion error if this object is read-only.

    Arguments:
    name -- name of the VDOService
    vdo -- the VDOService to add or replace
    replace -- if True, any existing VDOService will be replaced
    Returns: False if the VDOService exists and replace is False,
      True otherwise
    """
    self._assertCanModify()
    self.log.debug("Adding vdo \"{0}\" to configuration".format(name))
    if not replace and self.haveVdo(name):
      return False
    self._vdos[name] = vdo
    self._dirty = True
    return True

  ######################################################################
  def asYAMLForUser(self):
    """Returns the configuration's YAML representation to present to users.
    """
    s = yaml.dump({ "filename" : self.filepath,
                    "config"   : self},
                  default_flow_style = False)
    return s.replace("!!python/unicode ", "")


  ######################################################################
  @property
  def filepath(self):
    """Returns the file path of the configuration file."""
    return self._filename

  ######################################################################
  def getAllVdos(self):
    """Retrieves a list of all known VDOs."""
    return self._vdos

  ######################################################################
  def getVdo(self, name):
    """Retrieves a VDO by name."""
    vdo = None
    try:
      vdo = self._vdos[name]
    except KeyError:
      raise ArgumentError(_("VDO volume {0} not found").format(name))
    return vdo

  ######################################################################
  def haveVdo(self, name):
    """Returns True if we have a VDO with a given name."""
    return name in self._vdos

  ######################################################################
  def isDeviceConfigured(self, device):
    """Returns a boolean indicating if the configuration contains a VDO using
    the specified device.

    Both the specified device and the device from the vdos present in the
    configuration are fully resolved for the check.
    """
    device = os.path.realpath(device)
    for vdo in self._vdos:
      if device == os.path.realpath(self._vdos[vdo].device):
        return True
    return False

  ######################################################################
  def persist(self):
    """Writes out the Configuration if necessary.

    If the Configuration is read-only or has not been modified, this
    method will silently return. If Command.noRunMode is True, any
    new Configuration will be printed to stdout instead of the file.

    This method will generate an assertion failure if the configuration
    file is not open.
    """
    if self._readonly:
      return
    if not self._dirty:
      self.log.debug("Configuration is clean, not persisting")
      return

    self.log.debug("Writing configuration to {0}".format(self.filepath))

    if self._empty():
      self._removeFile()
      return

    s = yaml.dump({"config" : self}, default_flow_style = False)

    if Command.noRunMode():
      print(_("New configuration (not written):"))
      print(s)
      self._dirty = False
      return

    newFile = self.filepath + ".new"
    if os.path.exists(newFile):
      os.remove(newFile)
    with open(newFile, 'w') as fh:
      # Write the warning about not editing the file.
      fh.write(
       "####################################################################")
      fh.write(os.linesep)
      fh.write("# {0}".format(
        _("THIS FILE IS MACHINE GENERATED. DO NOT EDIT THIS FILE BY HAND.")))
      fh.write(os.linesep)
      fh.write(
       "####################################################################")
      fh.write(os.linesep)

      # Write the configuration, flush and sync.
      fh.write(s)
      fh.flush()
      os.fsync(fh)
    os.rename(newFile, self.filepath)
    self._fsyncDirectory()
    self._dirty = False

  ######################################################################
  def removeVdo(self, name):
    """Removes a VDO by name."""
    self._assertCanModify()
    del self._vdos[name]
    self._dirty = True

  ######################################################################
  def status(self):
    """Returns a dictionary representing the status of this object.
    """
    status = {}

    st = None
    try:
      st = os.stat(self.filepath)
      status[_("File")] = self.filepath
    except OSError as ex:
      if ex.errno != errno.ENOENT:
        raise
      status[_("File")] = _("does not exist")

    if st is None:
      status[_("Last modified")] = _("not available")
    else:
      status[_("Last modified")] = time.strftime('%Y-%m-%d %H:%M:%S',
                                                 time.localtime(st[ST_MTIME]))

    return status

  ######################################################################
  # Overridden methods
  ######################################################################
  @classmethod
  def _yamlMakeInstance(cls):
    return cls("/dev/YAMLInstance")

  ######################################################################
  @property
  def _yamlAttributeKeys(self):
    return ["version", "vdos"]

  ######################################################################
  @property
  def _yamlData(self):
    data = super(Configuration, self)._yamlData
    data["version"] = self._schemaVersion
    data["vdos"] = self._vdos
    return data

  ######################################################################
  def _yamlSetAttributes(self, attributes):
    super(Configuration, self)._yamlSetAttributes(attributes)
    self.version = attributes["version"]
    self.vdos = attributes["vdos"]

  ######################################################################
  @property
  def _yamlSpeciallyHandledAttributes(self):
    specials = super(Configuration, self)._yamlSpeciallyHandledAttributes
    specials.extend(["version", "vdos"])
    return specials

  ######################################################################
  def _yamlUpdateFromInstance(self, instance):
    super(Configuration, self)._yamlUpdateFromInstance(instance)

    self._schemaVersion = instance.version
    self._vdos = instance.vdos
    for vdo in self._vdos:
      self._vdos[vdo].setConfig(self)

  ######################################################################
  def __init__(self, filename, readonly=True, mustExist=False):
    """Construct a Configuration.

    Args:
      filename (str): The path to the XML configuration file

    Kwargs:
      readonly (bool): If True, the configuration is read-only.
      mustExist (bool): If True, the configuration file must exist.

    Raises:
      ArgumentError
    """
    super(Configuration, self).__init__()
    self._vdos = {}
    self._filename = os.path.abspath(filename)
    self._readonly = readonly
    self._dirty = False
    self._mustExist = mustExist
    self._schemaVersion = 0x20170907
    if not self.yaml_has_unicode_ctors:
      self._fix_constructors()
      self.yaml_has_unicode_ctors = True
    if self._mustExist and not os.path.exists(self.filepath):
      raise ArgumentError(_("Configuration file {0} does not exist.").format(
          self.filepath))
    mode = 'r' if readonly else 'a+'
    try:
      if os.path.exists(filename):
        if os.path.getsize(filename) != 0:
          with open(filename, mode) as fh:
            self._read(fh)
    except IOError as msg:
      raise ArgumentError(str(msg))

  ######################################################################
  def __str__(self):
    return "{0}({1})".format(type(self).__name__, self.filepath)

  ######################################################################
  # Protected methods
  ######################################################################
  def _assertCanModify(self):
    """Asserts that mutative operations are allowed on this object."""
    assert not self._readonly, "Configuration is read-only"

  ######################################################################
  def _empty(self):
    """Returns True if this configuration is empty."""
    return len(self._vdos) == 0

  ######################################################################
  def _fsyncDirectory(self):
    """Open and issue an fsync on the directory containing the config file.
    """
    dirname = os.path.dirname(self.filepath)
    if Command.noRunMode():
      runCommand(['fsync', dirname])
      return
    fd = os.open(dirname, os.O_RDONLY)
    try:
      os.fsync(fd)
    finally:
      os.close(fd)

  ######################################################################
  def _read(self, fh):
    """Reads in a Configuration from a file."""
    self.log.debug("Reading configuration from {0}".format(self.filepath))
    try:
      fh.seek(0, 0)
      conf = yaml.safe_load(fh)
    except yaml.scanner.ScannerError:
      raise BadConfigurationFileError(_("Not a valid configuration file"))

    # Because we do indirection instantiation from the YAML load we need to
    # call _yamlUpdateFromInstance().
    try:
      config = conf["config"]
    except KeyError:
      raise BadConfigurationFileError(_("Not a valid configuration file"
                                        " (missing 'config' section)"))
    except Exception as ex:
      self.log.debug("Not a valid configuration file: {0}".format(ex))
      raise BadConfigurationFileError(_("Not a valid configuration file"))

    try:
      self._yamlUpdateFromInstance(config)
    except Exception as ex:
      self.log.debug("Not a valid configuration file: {0}".format(ex))
      raise BadConfigurationFileError(_("Not a valid configuration file"))

    self._dirty = False
    return 0

  ######################################################################
  def _removeFile(self):
    """Deletes the current configuration file.
    In noRun mode, pretend that we're doing an rm of the file."""
    if Command.noRunMode():
      runCommand(['rm', self.filepath])
      return

    if os.path.exists(self.filepath):
      os.remove(self.filepath)
      self._fsyncDirectory()

    try:
      with FileLock(self.singletonLock, "r+") as f:
        del Configuration.modifiableSingltons[self.filepath]
    except KeyError:
      pass

  ######################################################################
  @classmethod
  def _validateVersion(cls, ver):
    """Checks a configuration file schema version string against the list
    of supported schemas.

    Args:
      ver (str): the schema version string to check

    Raises:
      BadConfigurationFileError: version not supported.
    """
    if ver not in cls.supportedSchemaVersions:
      raise BadConfigurationFileError(_(
          "Configuration file version {v} not supported").format(v=ver))
