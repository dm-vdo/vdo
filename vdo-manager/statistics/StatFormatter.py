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
  DisplaySpec

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/statistics/StatFormatter.py#1 $
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import sys

# Define a utility function, isstr(), that returns true if its argument
# is a string type. Avoids depending on the six library to test for
# strings in Python 2/3 code.
try:
    # Throws an exception if basestring isn't available
    basestring
    def isstr(s):
        return isinstance(s, basestring)
except NameError:
    def isstr(s):
        return isinstance(s, str)

class StatFormatter(object):
  """
  An object which formats labeled values. Formatters are able to
  represent all of the verbose formatting for vdoStats and vdoMonitor
  (the df-style formatting in vdoStats is done by hand).

  A formatter is specified by supplying a description of the format for each
  level of the hierarchy of labeled values. If the values have more levels than
  the formatter has specified, the final level specification will be applied
  to all remaining value levels.

  A level specification is specified as a hash with any of the following keys:

    displayFilter: A function which takes a LabeledValue and return True if
                   that value (and its children) should be displayed. Defaults
                   to a tautology.
    indent:        The additional indentation to apply from this level down.
                   Defaults to no additional indentation.
    joiner:        The string to use for joining label-value pairs at the
                   current level. In YAML mode, defaults to a newline,
                   otherwise defaults to a space.
    namer:         The way of naming the current level. Can be any of these:
                   None:   Don't name this level
                   list:   A list of indexes into the value list at this
                           level, the name of the level will be the
                           concatenation of the values at those indexes.
                           If the first element of the list is a string, it
                           will be used as a prefix for the name.
                   '=':    Use the label of the value at this level as the
                           name, use an equals sign to connect the name to the
                           values
                   '+':    Use the label of the value at this level as the
                           name, regardless of the formatter's mode
                   string: Any other string will be used as the name
                   True:   Any other true value will use the label as the name
                           if the mode indicates that the level should be named
                           and otherwise won't name the level
                           (non-hierarchical, multivalued levels don't get
                           named).
  """
  def __init__(self, displayLevels, hierarchical=True, yaml=True):
    """
    Create a new formatter.

    :param displayLevels: An array of hashes describing how to format each
                          level of the labled values to be formatted.
    :param hierarchical:  If True, indentation will be increased at each level
                          otherwise, indentation will only be modified at
                          levels which specify the 'indent' parameter. Defaults
                          to True.
    :param yaml:          Whether to output as YAML or not. Defaults to True.
    """
    self.hierarchical  = hierarchical
    self.yaml          = yaml
    self.spec          = None

    # build the DisplaySpecs from the bottom up to set child pointers.
    displayLevels.reverse()
    for level in displayLevels:
      self.spec = DisplaySpec(self, self.spec, level)

    # walk back down setting the indentation
    s = self.spec
    indent = ''
    while (True):
      indent = s.setIndentation(indent)
      if s.child is None:
        # Set the child of the bottom level to be itself
        s.child = s
        break
      s = s.child

  def format(self, lv):
    """
    Format labeled values.

    :param lv: The values to format

    :return: The formatted value string
    """
    return self.spec.format(lv)

  def output(self, lv):
    """
    Format labeled values and print the result.

    :param lv: The values to format
    """
    try:
      print(self.format(lv))
      sys.stdout.flush()
    except IOError:
      # Someone must have closed stdout. Don't die.
      pass

class DisplaySpec(object):
  """
  An object which formats a single level of labeled values.
  """
  def __init__(self, formatter, child, parameters):
    """
    Create a new display specification.

    :param formatter  : The formatter which owns this specification.
    :param child      : The formatter for the next level down.
    :param parameters : A dict which defines the format at this level. Valid
                        keys are documented in the header of this file.
    """
    self.formatter       = formatter
    self.parent          = None
    self.child           = child
    self.displayFilter   = parameters.get('displayFilter', lambda lv: True)
    self.indent          = parameters.get('indent', '')
    self.joiner          = parameters.get('joiner',
                                          "\n" if formatter.yaml else ' ')
    self.nameJoiner      = parameters.get('nameJoiner',
                                          ': ' if formatter.yaml else ' ')
    self.namer           = self._setNamer(parameters.get('namer'))
    self.width           = None
    self.subWidth        = None
    if child:
      child.parent = self

  def _setNamer(self, nameType):
    """
    Set up the namer for this level from the specification.

    :param nameType: The type of namer to use

    :return: The specified namer function
    """
    if not nameType:
      return lambda lv: None

    if isinstance(nameType, list):
      if isstr(nameType[0]):
        name = [nameType[0], nameType[1:]]
      else:
        name = ['', nameType]
      return lambda lv: (name[0]
                         + ' '.join(str(lv.value[n].value) for n in name[1]))

    if isstr(nameType):
      if (nameType == '='):
        self.nameJoiner = '='
        return lambda lv: None if lv.isMultiValued() else lv.label

      if (nameType == '+'):
        return lambda lv: lv.label

      return lambda lv: nameType

    if self.formatter.hierarchical:
      return lambda lv: lv.label

    return lambda lv: None if lv.isMultiValued() else lv.label

  def getName(self, lv):
    """
    Get the name for a labeled value.

    :param lv: The value to name

    :return: The name for the value or None if the value should not be named
    """
    name = self.namer(lv)
    if not name:
      return None

    if self.formatter.yaml:
      nameWidth = max(self.width, len(name)) + 1
      return "{0}{1:<{2}}{3}{4}".format(self.indent, name, nameWidth,
                                        self.nameJoiner,
                                        "\n" if lv.isMultiValued() else '')

    return self.indent + name + self.nameJoiner

  def setIndentation(self, parentIndent):
    """
    Set the indentation for this level.

    :param parentIndent: The indentation of the level above us
    """
    self.indent = parentIndent + self.indent
    return self.indent

  def setWidth(self, lv):
    """
    Set the width of the labels at this level.

    :param lv: The value being formatted
    """
    if self.width:
      return

    if self.formatter.hierarchical:
      self.width    = self.parent.subWidth if self.parent else lv.width()
      self.subWidth = lv.subWidth(True)
      return

    if self.parent:
      self.width = self.subWidth = self.parent.subWidth
      return

    self.width    = lv.width()
    self.subWidth = lv.subWidth(False)

  def format(self, lv):
    """
    Recursively format a labeled value.

    :param lv: The value to format

    :return: The formatted value string
    """
    if not self.displayFilter(lv):
      return None

    self.setWidth(lv)
    name  = self.getName(lv)
    value = lv.format(self.child, self.joiner)
    return name + value if name else value
