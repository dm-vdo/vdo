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
  Field - base class for a field of a collection of statistics

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/statistics/Field.py#1 $
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from ctypes import *
from .LabeledValue import LabeledValue
import re

class Field(object):
  """
  Field is the base class for a field of a statistics structure.
  """
  decamelRE1  = re.compile(r'([A-Z][a-z])')
  decamelRE2  = re.compile(r'([a-z])([A-Z])')
  fieldNames = re.compile(r'\$([a-zA-Z0-9_]+)')

  @classmethod
  def _decamel(cls, string):
    """
    Convert a camel-cased string to a set of space separated, lower case words.

    Preserves uppercase acronyms, so 'launchVDOErrorCount' becomes
    'launch VDO error count'.

    :param string: The string to convert

    :return: The converted string
    """
    lowered = cls.decamelRE1.sub(lambda match: " " + match.group().lower(),
                                 string)
    return cls.decamelRE2.sub(lambda match: " ".join(match.groups()), lowered)

  def _generateLambda(self, string):
    """
    Convert a string describing how to derive a field's value into a lambda.

    :param string    The string to convert

    :return: An equivalent lambda
    """
    derivation = self.fieldNames.sub(r'parent.getSampleValue(stats, "\1")',
                                     string)
    return lambda stats, parent: eval(derivation)

  def __init__(self, name, cType, **kwargs):
    """
    Create a new field.

    :param name:     The name of the field
    :param cType:    The class representing the C representation for this field
                     when sampled via an ioctl
    :param **kwargs: Keyword arguments which may be:
                       available: Specifies python code to apply to other
                                  fields of the parent structure to decide
                                  whether this value is available. Defaults
                                  to True.
                       derived:   Specifies python code to apply to other
                                  fields of the parent structure to derive the
                                  value of this field. Defaults to None.
                       display:   If not True, this field will not be included
                                  in labeled output. Defaults to True.
                       label:     The label for this field. If unspecified, the
                                  label will be derived from the field name.
                       length:    if > 1, indicates this field is an array of
                                  the specified cType, otherwise is is a
                                  scalar. Defaults to 1.
    """
    self.name      = name
    self.length    = kwargs.pop('length', 1)
    self.cType     = cType * self.length if (self.length > 1) else cType
    self.display   = kwargs.pop('display', True)
    self.label     = (kwargs.pop('label', self._decamel(self.name))
                      if self.display else None)

    self.available = self._generateLambda(kwargs.pop('available', "True"))

    # While all stats have C types, not all stats are present in the
    # struct returned via the ioctl.
    derived       = kwargs.pop('derived', None)
    self.inStruct = (derived is None)

    defaultValue  = "getattr(stats, '{name}')".format(name = self.name)
    self.getValue = self._generateLambda(derived if derived else defaultValue)

    if kwargs:
      raise Exception("unknown arguments to Field: {0}".format(kwargs.keys()))

  def extractSample(self, stats, parent):
    """
    Extract the value for this field from a sample.

    :param stats: The raw stats returned from an ioctl
    :param parent: The parent of this field

    :return: The value of this field in the current sample
    """
    if not self.available(stats, parent):
      return NotAvailable()

    return self.getValue(stats, parent)

  def labeled(self, sample, prefix):
    """
    Label a sampled value for this field.

    :param sample: The sampled field value
    :param prefix: The prefix for the label

    :return: A LabeledValue for a value of this field
    """
    return LabeledValue.make(prefix + self.label, sample)

# base integer
class IntegerField(Field):
  """
  Base class for fields which are integer types.
  """
  def extractSample(self, stats, parent):
    """
    :inherit:
    """
    return int(super(IntegerField, self).extractSample(stats, parent))

# basic integer types
class BoolField(IntegerField):
  def __init__(self, name, **kwargs):
    super(BoolField, self).__init__(name, c_byte, **kwargs)

  def extractSample(self, stats, parent):
    return (super(BoolField, self).extractSample(stats, parent) != 0)

class Uint8Field(IntegerField):
  def __init__(self, name, **kwargs):
    super(Uint8Field, self).__init__(name, c_byte, **kwargs)

class Uint32Field(IntegerField):
  def __init__(self, name, **kwargs):
    super(Uint32Field, self).__init__(name, c_uint, **kwargs)

class Uint64Field(IntegerField):
  def __init__(self, name, **kwargs):
    super(Uint64Field, self).__init__(name, c_ulonglong, **kwargs)

# base float
class FloatingPointField(Field):
  """
  Base class for fields which are floating point types.
  """
  def extractSample(self, stats, parent):
    """
    :inherit:
    """
    return float(super(FloatingPointField, self).extractSample(stats, parent))

# basic float type
class FloatField(FloatingPointField):
  def __init__(self, name, **kwargs):
    super(FloatField, self).__init__(name, c_float, **kwargs)

# the basic string type
class StringField(Field):
  def __init__(self, name, **kwargs):
    super(StringField, self).__init__(name, c_char, **kwargs)

# not available
class NotAvailable(int):
  """
  A value for numeric statistics which are currently not available; prints
  as 'N/A'.
  """
  def __str__(self):
    return "N/A"

  def __repr__(self):
    return "NotAvailable"

  def __add__(self, other):
    return self

  def __radd__(self, other):
    return self

  def __sub__(self, other):
    return self

  def __rsub__(self, other):
    return self

  def __mul__(self, other):
    return self

  def __rmul__(self, other):
    return self

  def __div__(self, other):
    return self

  def __rdiv__(self, other):
    return self

  def __int__(self):
    return self

  def __format__(self, spec):
    return ("{0:" + spec + "}").format(str(self))
