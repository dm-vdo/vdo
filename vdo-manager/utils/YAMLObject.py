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

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/utils/YAMLObject.py#2 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

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
  # Overridden methods
  ######################################################################
  @classmethod
  def from_yaml(cls, loader, node):
    """Constructs and returns an instance from its YAML representation.

    Returns:
      instance constructed from YAML representation.
    """
    instance = cls._yamlMakeInstance()
    yield instance
    mapping = loader.construct_mapping(node)
    instance._yamlSetAttributes(mapping)

  ######################################################################
  @classmethod
  def to_yaml(cls, dumper, data):
    """Returns a YAML representation of the instance.

    Returns:
      YAML representation of instance
    """
    yamlData = data._yamlData
    return dumper.represent_mapping(data.yaml_tag, yamlData)

  ######################################################################
  def __init__(self):
    super(YAMLObject, self).__init__()
    self._preservedExtraAttributes = {}

  ######################################################################
  # Protected methods
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
    filtered through the list of keys returned from _yamlAttributeKeys
    excluding any that require special handling (subclasses must override this
    method to add those) and adds any entries from the preserved extra
    attributes.

    Returns:
      dictionary of data to use in generating the instance's YAML
      representation
    """
    data = dict([(key, value)
                  for key, value in list(self.__dict__.items())
                  if ((key in self._yamlAttributeKeys)
                    and (key not in self._yamlSpeciallyHandledAttributes))])
    data.update(self._preservedExtraAttributes)
    return data


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
    to handle the attributes appropriately.  The subclass must also account
    for the possibility that the attributes it receives may not contain the
    entirety of such specially handled attributes; in other words, it must
    check for each attribute's existence in the attributes dictionary it
    receives.  Any unspecified attributes must be given a default value in
    whatever manner the subclass chooses.

    Any entries in the specified attributes which are unknown to the entity
    being constructed are preserved for later use in generating YAML output.

    Arguments:
      attributes (dictionary) - the attributes from the YAML representation
    """
    extra = dict([(key, value)
                    for key, value in attributes.items()
                      if key not in self._yamlAttributeKeys])
    self._preservedExtraAttributes.update(extra)

    keys = [key for key in attributes.keys()
                if (key in self._yamlAttributeKeys)
                  and (key not in self._yamlSpeciallyHandledAttributes)]
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

  ######################################################################
  def _yamlUpdateFromInstance(self, instance):
    """Updates the current object (self) from the specified instance.
    This is for classes that perform indirect construction using the results
    of the YAML load.  Instances of such classes are required to invoke this
    method themselves.

    The specified instance must be the same class (or a subclass) of the
    current object to guarantee that any attributes the current object attempts
    to update will be available from the instance.

    Arguments:
      instance (YAMLObject) - object source for update

    Raises:
      TypeError - specified instance is not the same class (or a subclass)
                  of the current object
    """
    if (not isinstance(instance, type(self))):
      raise TypeError(_("attempt to update from incompatible type"))

    self._preservedExtraAttributes.update(instance._preservedExtraAttributes)

  ######################################################################
  @staticmethod
  def _construct_yaml_str(_loader, node):
    "Provide a constructor for Python Unicode objects."
    return node.value

  ######################################################################
  @staticmethod
  def _fix_constructors():
    yaml.Loader.add_constructor('tag:yaml.org,2002:python/unicode',
                                YAMLObject._construct_yaml_str)
    yaml.SafeLoader.add_constructor('tag:yaml.org,2002:python/unicode',
                                    YAMLObject._construct_yaml_str)
