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

  YAMLObject - Provides mapping to/from YAML.

  $Id: //eng/vdo-releases/magnesium-rhel7.5/src/python/vdo/utils/YAMLObject.py#1 $

"""

import gettext
import yaml

gettext.install("utils")

########################################################################
class YAMLObject(yaml.YAMLObject):
  """Provides conversion of objects to and from YAML representations.

  The attributes that are included in the YAML representation are taken
  from an instance's __dict__ filtered through the list returned from the
  _yamlAttributeKeys property.

  The list from the _yamlAttributeKeys property is used to check for missing
  attributes when generating the YAML representation.  It is also used
  to check for missing or extraneous attributes when constructing an instance
  from the YAML representation.
  Subclasses must override _yamlAttributeKeys.

  Subclasses must specify the class attribute yaml_tag which indicates the type
  of the instance in the YAML representation.

  Class attributes:
    yaml_tag (unicode string) - YAML representation identfifier;
                                must be specified by subclasses
    yaml_loader               - The loader to use; set to yaml.SafeLoader
                                to allow yaml.load_safe() to instantiate
                                objects
  """
  yaml_loader = yaml.SafeLoader

  ######################################################################
  @classmethod
  def from_yaml(cls, loader, node):
    """Constructs and returns an instance from its YAML representation.

    If there are extraneous or missing attributes in the YAML representation
    KeyError is raised.

    Raises:
      KeyError  - extraneous or missing attributes in YAML representation

    Returns:
      instance constructed from YAML representation.
    """
    instance = cls._yamlMakeInstance()
    yield instance
    mapping = loader.construct_mapping(node)
    # Check for extraneous or missing keys.
    if set(mapping.keys()) != set(instance._yamlAttributeKeys):
      raise KeyError(_("extraneous or missing YAML attributes"))
    instance._yamlSetAttributes(mapping)

  ######################################################################
  @classmethod
  def to_yaml(cls, dumper, data):
    """Returns a YAML representation of the instance.

    If there are missing attributes for the YAML representation KeyError is
    raised.

    Raises:
      KeyError  - missing attributes for YAML representation

    Returns:
      YAML representation of instance
    """
    yamlData = data._yamlData
    # Check for extraneous or missing data.
    if set(yamlData.keys()) != set(data._yamlAttributeKeys):
      raise KeyError(_("extraneous or missing YAML attributes"))
    return dumper.represent_mapping(data.yaml_tag, yamlData)

  ######################################################################
  @classmethod
  def _yamlMakeInstance(cls):
    """Returns an instance of the class specifying placeholder values for
    any arguments required by the __init__ method.

    The default implementation is to instantiate without arguments.
    Subclasses must override if their __init__ method takes arguments.
    """
    return cls()

  ######################################################################
  @property
  def _yamlData(self):
    """Returns a dictionary containing the data to include in generating
    the instance's YAML representation.

    The base implementation uses the contents of the instance's __dict__
    filtered through the list of keys returned from _yamlAttributeKeys.

    Returns:
      dictionary of data to use in generating the instance's YAML
      representation
    """
    return dict([(key, value)
                  for key, value in self.__dict__.iteritems()
                  if ((key in self._yamlAttributeKeys)
                    and (key not in self._yamlSpeciallyHandledAttributes))])

  ######################################################################
  @property
  def _yamlAttributeKeys(self):
    """Returns a list of the keys for the attributes to include in the
    instance's YAML representation.

    Must be overridden by subclasses.

    Returns:
      list of keys for attributes to include in the instance's YAML
      representation
    """
    raise NotImplementedError

  ######################################################################
  def _yamlSetAttributes(self, attributes):
    """Sets the instance's internal attributes from the YAML attributes.

    By default, the attributes are set via invoking setattr() targeted
    to the instance.  If a subclass uses different internal names or
    computes the attributes in some way it must override this method
    to handle the attributes appropriately.

    Arguments:
      attributes (dictionary) - the attributes from the YAML representation
    """
    keys = [key for key in attributes.keys()
                if key not in self._yamlSpeciallyHandledAttributes]
    for key in keys:
      setattr(self, key, attributes[key])

  ######################################################################
  @property
  def _yamlSpeciallyHandledAttributes(self):
    """Returns a list of attributes that are specially handled by subclasses.
    These attributes are skipped by _yamlData and _yamlSetAttributes
    implemented by YAMLObject.

    Subclasses must override both _yamlData and _yamlSetAttributes to handle
    these attributes.
    """
    return []
