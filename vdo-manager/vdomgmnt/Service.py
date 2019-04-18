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
  Service - Abstract superclass for services

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/Service.py#2 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from . import ExitStatus
from vdo.utils import YAMLObject

class ServiceError(ExitStatus, Exception):
  """Base class for service errors.
  """

  ######################################################################
  # Public methods
  ######################################################################

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, msg = "", *args, **kwargs):
    super(ServiceError, self).__init__(*args, **kwargs)
    self._msg = msg

  ######################################################################
  def __str__(self):
    return self._msg

  ######################################################################
  # Protected methods
  ######################################################################

########################################################################
class Service(YAMLObject):
  """Superclass for services.

  Every subclass of Service controls a service (such as an Albireo
  index or a VDO target) managed by this command. The create/remove/
  have methods are one-time operations that do things like 'albcreate'
  that are persistent, while start/stop/running are used to control
  the availability of the service, either manually or automatically at
  system boot and shutdown. The control commands are idempotent, and
  return values specified as exit codes for /etc/init.d scripts
  specified in the LSB.

  Methods:
    getName  (method on Service) returns a name for the object
    create   creates the service; done once, paired with 'remove'
    remove   removes the service
    have     returns True if the service has been created
    start    starts the service; idempotent; run at system boot
    stop     stops the service; idempotent; run at shutdown
    running  returns True if the service is running
    getKeys  returns a list of the keys to be stored in the
             configuration file
    status   returns the status of the service in YAML format
  """
  yaml_tag = "!Service"

  ######################################################################
  # Public methods
  ######################################################################
  @staticmethod
  def getKeys():
    """Returns a list of keys to be stored in the configuration file."""
    return []

  ######################################################################
  def getName(self):
    """Returns the name of a Service, as a string."""
    return self._name

  ######################################################################
  # Overridden methods
  ######################################################################
  @property
  def _yamlAttributeKeys(self):
    keys = ["name"]
    keys.extend(self.getKeys())
    return keys

  ######################################################################
  @property
  def _yamlData(self):
    data = super(Service, self)._yamlData
    data["name"] = self.getName()
    return data

  ######################################################################
  def _yamlSetAttributes(self, attributes):
    super(Service, self)._yamlSetAttributes(attributes)
    self._name = attributes["name"]

  ######################################################################
  @property
  def _yamlSpeciallyHandledAttributes(self):
    specials = super(Service, self)._yamlSpeciallyHandledAttributes
    specials.extend(["name"])
    return specials

  ######################################################################
  def __init__(self, name):
    super(Service, self).__init__()
    self._name = name

  ######################################################################
  def __str__(self):
    return "{0}({1})".format(type(self).__name__, self.getName())

  ######################################################################
  # Protected methods
  ######################################################################
  def _reprAttribute(self, key):
    """Returns a boolean indicating if the entry represented by 'key' from the
       instance's __dict__ should be included in the __repr__ result.

    Arguments:
      key (str):  key from instance's __dict__
    Returns:
      bool: True, if the __dict__ entry with the specified key should be
                  included in the __repr__ result
            False, otherwise
    """
    return not key.startswith('_')
