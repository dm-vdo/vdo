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
  LabeledValue - A sampled statistic with a label. Used for formatting stats
  output.

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/statistics/LabeledValue.py#2 $
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

class LabeledValue(object):
  """
  An object which represents a labeled statistic value or a labeled collection
  of other LabeledValues.
  """

  @staticmethod
  def make(label, value):
    """
    Create a new labeled value.

    :param label: The label
    :param value: The value

    :return: The new labeled value. If the supplied value is a list, the
             returned object will be a LabeledValueList, otherwise it
             will be a LabeledValue.
    """
    if isinstance(value, list):
      return LabeledValueList(label, value)
    return LabeledValue(label, value)

  def __init__(self, label, value):
    """
    Create a new labeled value.

    :param label: The label
    :param value: The value
    """
    self.label = label
    self.value = value

  def isMultiValued(self):
    """
    Check whether this is a collection or a single value.

    :return: True if this is a collection of values
    """
    return False

  def width(self):
    """
    Get the width of the label for this value.

    :return: The width of the label
    """
    return len(self.label)

  def subWidth(self, hierarchical):
    """
    Get the maximum width of the labels of this value and/or all of
    its sub-values.

    :param hierarchical: Whether the format mode is hierarchical or not

    :return: The width of this value or its immediate sub-values if
              hierarchical, otherwise, the width of this value or its full tree
              of sub-values.
    """
    return self.width()

  def hasSubValue(self, index):
    """
    Check whether this value has a subvalue for the given index.

    :param index: The index into the subvalue list to check

    :return: True if the indexed subvalue exists
    """
    return False

  def format(self, displaySpec=None, joiner=None):
    """
    Format this value and any of its children.

    :param displaySpec: The display specification
    :param joiner:      The string for joining subvalues
    """
    if isinstance(self.value, bytes):
      return self.value.decode("ASCII")
    return str(self.value)

class LabeledValueList(LabeledValue):
  """
  An object representing a labeled collection of labeled values.
  """
  def subWidth(self, hierarchical):
    """
    :inherit:
    """
    if (hierarchical):
      return max(v.width() for v in self.value)
    return max(v.subWidth(False) for v in self.value)

  def isMultiValued(self):
    """
    :inherit:
    """
    return True

  def hasSubValue(self, index):
    """
    :inherit:
    """
    try:
      return (self.value[index].format() != '')
    except IndexError:
      return False

  def format(self, displaySpec, joiner):
    """
    :inherit:
    """
    return joiner.join(filter(None,
                              [displaySpec.format(lv) for lv in self.value]))
