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
  Logger - VDO manager logging

  $Id: //eng/vdo-releases/magnesium-rhel7.5/src/python/vdo/utils/Logger.py#1 $

"""
import logging
import logging.handlers
import os
import sys
from types import MethodType

class Logger(object):
  """Wrappers and configuration methods for the Python logger.

  Attributes:
    logfile (string): the path to the logfile if specified.
    myname (string):  name of the command being run.
    quiet (bool):     if True, don't print to stdout.
  """
  myname = os.path.basename(sys.argv[0])
  quiet = False
  logfile = None

  ######################################################################
  # Public methods
  ######################################################################
  @classmethod
  def announce(cls, logger, msg):
    """Print a status message to stdout and log it as well."""
    if not cls.quiet:
      print(msg)
    logger.info(msg)

  ######################################################################
  @classmethod
  def configure(cls, name, logfile = None, debug = False):
    """Configure the logging system according to the arguments."""
    cls.myname  = name
    cls.logfile = logfile
    debugging   = debug
    formatBase  = ': %(levelname)s - %(message)s'
    debugBase   = (': %(name)s' if debugging else '') + formatBase

    logger = logging.getLogger()
    logger.setLevel(logging.NOTSET)

    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(logging.Formatter(cls.myname + debugBase))
    handler.setLevel(logging.DEBUG if debugging else logging.WARNING)
    logger.addHandler(handler)

    if cls.logfile is not None:
      if os.path.exists(cls.logfile) and not os.path.isfile(cls.logfile):
        # Support /dev/stderr and the like.
        handler = logging.FileHandler(cls.logfile)
      else:
        handler = logging.handlers.RotatingFileHandler(cls.logfile,
                                                       maxBytes=10*1024*1024,
                                                       backupCount=5)
      formatter = logging.Formatter('%(asctime)s %(name)s' + formatBase)
      handler.setFormatter(formatter)
      handler.setLevel(logging.DEBUG if debugging else logging.INFO)
      logger.addHandler(handler)

    try:
      handler = logging.handlers.SysLogHandler(address='/dev/log')
      handler.setFormatter(logging.Formatter(cls.myname + formatBase))
      handler.setLevel(logging.WARNING)
      logger.addHandler(handler)
    except Exception as ex:
      logger.warn('Unable to configure logging for rsyslog: {0}'.format(ex))

  ######################################################################
  @classmethod
  def getLogger(cls, name):
    """Returns a Python logger decorated with the announce method."""
    logger = logging.getLogger(name)
    logger.announce = MethodType(cls.announce, logger, logger.__class__)
    return logger

  ######################################################################
  @classmethod
  def loggingOptions(cls):
    """Return a list of strings containing the logging options specified."""
    options = [ ]
    if cls.logfile is not None:
      options.append("--logfile=" + str(cls.logfile))
    return options

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(Logger, self).__init__()
