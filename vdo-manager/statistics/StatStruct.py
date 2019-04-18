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
  StatStruct -- classes for sampling statistics from a VDO via ioctls

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/statistics/StatStruct.py#1 $
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from ctypes import *
import collections
import fcntl
import os
import sys

from .Field import Field
from .LabeledValue import LabeledValue

class Samples(object):
  """
  An object which represents a collection of samples from a VDO.
  """
  def __init__(self, assays, device, mustBeVDO):
    """
    Create a new set of samples by sampling a VDO device.

    :param assays:    The types of samples to take
    :param devices:   The device to sample (a dictionary containing
                      the user-supplied name and the name to use for sampling)
    :param mustBeVDO: If set to False, errors resulting from the device not
                      being a VDO will be suppressed
    """
    self.device = device["user"]
    self.samples = [assay.sample(os.path.basename(device["sample"]))
                    for assay in assays]

  def getDevice(self):
    """
    Get the name of the device which was sampled.

    :return: The name of the device
    """
    return self.device

  def getSamples(self):
    """
    Get the list of samples for the device.

    :return: The list of samples
    """
    return self.samples

  @staticmethod
  def assay(assays, device, mustBeVDO=True):
    """
    Assay a device.

    :param assays:    The types of samples to take
    :param devices:   The device to sample (a dictionary containing
                      the user-supplied name and the name to use for sampling)
    :param mustBeVDO: If set to False, errors resulting from the device not
                      being a VDO will be suppressed

    :return: The results of the assays or None if the device is not a VDO and
             mustBeVDO is set to False
    """
    try:
      return Samples(assays, device, mustBeVDO)
    except IOError as ioe:
      user = device["user"]
      if (ioe.errno == 22):
        if mustBeVDO:
          raise Exception("Device {0} is not a VDO".format(user))
        return None
      raise Exception("Error sampling device {0}: {1}".format(user, ioe))

  @staticmethod
  def assayDevices(assays, devices, mustBeVDO=True):
    """
    Assay a list of devices.

    :param assays:    The types of samples to take
    :param devices:   The devices to sample (a list of dictionaries containing
                      the user-supplied name and the name to use for sampling)
    :param mustBeVDO: If set to False, errors resulting from the device not
                      being a VDO will be suppressed

    :return: The results of the assays or None if the device is not a VDO and
             mustBeVDO is set to False
    """
    return filter(None, [Samples.assay(assays, device, mustBeVDO)
                         for device in devices])

  @staticmethod
  def samplingDevice(user, sample):
    """
    Returns a dictionary used for sampling purposes.

    The dictionary is structured as:
      { "user"   : <user-specified name>,
        "sample" : <sample name> }

    The user-specified name is used for display purposes.
    The sample name is used to perform the actual sampling.
    The two names may be identical.

    :param user:    user-specified name
    :param sample:  the name to use for sampling

    :return:  A sampling dictionary.
    """
    return { "user" : user, "sample" : sample }

class Sample(object):
  """
  An object which represents a single sample (ioctl) of a VDO.
  """
  def __init__(self, statStruct, sample):
    """
    Create a new sample.

    :param statStruct: The structure representing the type of this sample
    :param sample:     The sampled values
    """
    self.statStruct = statStruct
    self.sample     = sample

  def getType(self):
    """
    Get the object representing the type of this sample.

    :return: The StatStruct which represents the type of this sample
    """
    return self.statStruct

  def labeled(self):
    """
    Get the sampled values as a collection of LabeledValues.

    :return: A LabeledValue representing the sampled values
    """
    return self.statStruct.labeled(self.sample)

  def getStat(self, statName):
    """
    Get the value of a named statistic.

    :param statName:     The name of the statistic, either as a string, or as
                         a list of strings, one for each level of the sample
                         hierarchy

    :return: The value of the named statistic
    """
    if not isinstance(statName, list):
      return self.sample[statName]

    stats = self.sample
    for name in statName[:-1]:
      stats = stats[name]
    return stats[statName[-1]]

  def statEqual(self, other, statName):
    """
    Check whether the value of a given statistic is the same in this sample
    and some other sample.

    :param other: The other sample
    :param statName: The name of the statistic as would be specified to
                     getStat()

    :return: True if the value of the named statistic is the same in this and
             the other sample
    """
    return (self.getStat(statName) == other.getStat(statName))

class StatStruct(Field):
  """
  Base class for objects representing a VDO statistics structure. This object
  can be used to sample a VDO via an ioctl and convert the result from the
  C format in the ioctl to a Sample object.
  """

  """
  The dict of C classes
  """
  cClasses = {}

  def __init__(self, name, fields, **kwargs):
    """
    Create a new statistics structure.

    :param name:     The name of statistics structure
    :param fields:   A list of Field objects specifying the format of the C
                     structure returned by the ioctl
    :param **kwargs: Keyword args which may be:
                       labelPrefix: The prefix to prepend to the label for
                                    each field in this structure
                       ioctl:       The value of the ioctl to use for sampling
                                    a VDO
                       Field:       Any of the keyword arguments for the Field
                                    base class
    """
    labelPrefix      = kwargs.pop('labelPrefix', None)
    self.labelPrefix = labelPrefix + ' ' if (labelPrefix != None) else ''
    self.ioctl       = kwargs.pop('ioctl', None)
    self.procFile    = kwargs.pop('procFile', None)
    self.procRoot    = kwargs.pop('procRoot', None)
    self.fields      = fields
    super(StatStruct, self).__init__(name, self._getCClass(), **kwargs)
    self.fieldsByName = dict()
    for field in fields:
      self.fieldsByName[field.name] = field

  def _getCClass(self):
    """
    Get the Structure class which represents the C struct for a StatStruct
    or Field. If the Structure class hasn't yet been made for the given type,
    it will be created.

    :return: The class defined by the specified set of Fields
    """
    className = type(self).__name__ + '.c'
    cType     = self.cClasses.get(className)
    if not cType:
      fieldList = [(field.name, field.cType) for field in self.fields
                   if field.inStruct]
      cType = type(str(className), (Structure,), { '_fields_': fieldList });
      self.cClasses[className] = cType

    return cType

  def sample(self, name):
    """
    Get a sample from a VDO via an ioctl.

    :param name: The name of the proc directory from which to read

    :return: The sample
    """
    stats    = self.cType()
    procPath = os.path.join("/proc", self.procRoot, name, self.procFile)
    with open(procPath, 'rb') as fd:
      fd.readinto(stats)
    return self._extract(stats)

  def _extract(self, stats):
    """
    Extract the sampled values from the return of an ioctl call.

    :param stats: The structure returned from an ioctl

    :return: The sample as a Sample
    """
    sample = dict()
    for field in self.fields:
      sample[field.name] = field.extractSample(stats, self)
    return Sample(self, sample)

  def extractSample(self, stats, parent):
    """
    :inherit:
    """
    myStats = getattr(stats, self.name, stats)
    if (self.length > 1) and (myStats != stats):
      # The current field is actually an array, so make a list of the
      # extractions of each element of the array.
      return [self.extractSample(s, self) for s in myStats]

    # The current field is a struct which is not an array, so recursively
    # extract each sub-field of that struct
    sample = dict()
    for field in self.fields:
      sample[field.name] = field.extractSample(myStats, self)
    return sample

  def getSampleValue(self, stats, fieldName):
    """
    Get the value of one of the fields at the current level of the C structure
    from the ioctl.

    :param stats:     The current level of the C structure
    :param fieldName: The name of the field to get

    :return: The value of the specified field
    """
    return self.fieldsByName[fieldName].extractSample(stats, self)

  def labeled(self, sample, prefix = ''):
    """
    :inherit:
    """
    prefix += self.labelPrefix
    label   = prefix + self.label
    if isinstance(sample, list):
      # The current field is an array, so convert each array element
      labeledFields = [self.labeled(s, prefix) for s in sample]
    else:
      # The current field is a struct, so recursively convert each sub-field
      labeledFields = [field.labeled(sample[field.name], prefix)
                       for field in self.fields if field.display]

    return LabeledValue.make(label, labeledFields)
