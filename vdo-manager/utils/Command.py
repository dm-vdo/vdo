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
  Command - runs commands and manages their results

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/utils/Command.py#1 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from subprocess import Popen, PIPE
import copy
import logging
import os
import pipes
import time

class CommandError(Exception):
  """Exception raised to indicate an error running a command.

  Arguments:
    exitCode (int):   The exit code of the command
    stdout (string):  The output from the command
    stderr (string):  The error output from the command
    message (string): The (localized) error text; will be formatted with
                      the remaining arguments
    args:             Arguments for formatting the message
  """
  ######################################################################
  # Public methods
  ######################################################################
  def getExitCode(self):
    return self._exitCode

  ######################################################################
  def getStandardError(self):
    return self._stderr

  ######################################################################
  def logOutputs(self, logMethod):
    """Log the outputs of the failed command which generated this exception.

    Arguments:
      logMethod (callable): The method to log with
    """
    logMethod(self._stdout)
    logMethod(self._stderr)

  ######################################################################
  def setMessage(self, message, *args):
    """Set the error message in this exception.

    Arguments:
      message (string): The (localized) message text; will be formatted
                        with *args
      args:             Values to pass to the format of message
    """
    self._message = message.format(*args)

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, exitCode, stdout, stderr, message, *args):
    super(CommandError, self).__init__()
    self._exitCode = exitCode
    self._stdout   = stdout
    self._stderr   = stderr
    self.setMessage(message, args)

  ######################################################################
  def __str__(self):
    return self._message

########################################################################
class Command(object):
  """Command encapsulates shell commands, runs them, and manages the result.

  Attributes:
    noRun (bool): if True, don't run the command, and always succeed
    shell (bool): if True, run this command using shell -c
    verbose (int): if > 0, print commands to stdout before executing them
    _commandList (list): command and its arguments
  """
  defaultNoRun   = False
  defaultVerbose = 0
  log            = logging.getLogger('utils.Command')

  ######################################################################
  # Public methods
  ######################################################################
  @classmethod
  def noRunMode(cls):
    """Returns True iff Commands default to noRun."""
    return cls.defaultNoRun

  ######################################################################
  @classmethod
  def setDefaults(cls, verbose = False, noRun = False):
    """Sets the verbose and noRun default values.

    Arguments:
      verbose:  (boolean) If True, operate verbosely.
      noRun:    (boolean) If True, do not actually execute.
    """
    if noRun:
      cls.defaultNoRun   = noRun
      cls.defaultVerbose = True
    if verbose:
      cls.defaultVerbose = verbose

  ######################################################################
  def commandName(self):
    """Returns an identifier (argv[0]) for error messages."""
    return self._commandList[0]

  ######################################################################
  def run(self, **kwargs):
    """Run a command.

    Returns the output of running the command.

    Arguments:
      noThrow: If True, will return an empty string instead of throwing on
               error.
      retries: The number of times to try the command before giving up.
               Defaults to 1.
      shell:   Indicate that this is a shell command
      stdin:   If not None, the stream from which the command should take
               its input, defaults to None.
      strip:   If True, strip leading and trailing whitespace from the
               command output before returning it.

    Exceptions:
      CommandError: if the command failed and noThrow is False
    """
    retries = kwargs.get('retries', 1)
    stdin   = kwargs.get('stdin', None)
    if not self.shell:
      self.shell = kwargs.get('shell', False)
    commandLine = self._getCommandLine()
    if retries > 1:
      self.log.debug("Waiting for '{0}'".format(commandLine))

    try:
      for count in range(retries):
        if retries > 1:
          self.log.debug("  ... {0}/{1}".format(count, retries))
        if self.verbose > 0:
          print('    ' + commandLine)
          self.log.info(commandLine)
        if self.noRun:
          return
        try:
          output = self._execute(stdin)
          return output.strip() if kwargs.get('strip', False) else output
        except CommandError as e:
          if count == (retries - 1):
            if retries > 1:
              e.setMessage(_("{0}: timed out after {1} seconds"),
                           self.commandName(), retries)
            raise e
          time.sleep(1)
    except CommandError as e:
      if kwargs.get('noThrow', False):
        return ''
      raise e

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self, commandList, environment=None):
    super(Command, self).__init__()
    self.noRun        = Command.defaultNoRun
    self.shell        = False
    self.verbose      = Command.defaultVerbose
    self._commandList = commandList
    if environment:
      self.env = copy.deepcopy(os.environ)
      for var, value in environment.items():
        self.env[var] = value
    else:
      self.env = None

  ######################################################################
  def __str__(self):
    ' '.join(self._commandList)

  ######################################################################
  # Protected methods
  ######################################################################
  def _checkResults(self, exitCode=0, stdout='', stderr=''):
    """Sets the result values of this object. Raises an exception if there was
    an error, or returns the output of the command if there was not.

    Arguments:
      exitCode (int):    the process exit code
      stdout (str):      the standard output
      stderr (str):      the standard error
      logResults (bool): if True, the results will be logged

    Exceptions:
      CommandError: if exitCode is non-zero
    """
    try:
      if (exitCode == 0):
        self.log.debug(_("{0}: command succeeded").format(self.commandName()))
        return stdout

      failureType = _('exit status') if exitCode > 0 else _('signal')
      status = _("{0}: command failed, {1} {2}").format(self.commandName(),
                                                        failureType,
                                                        abs(exitCode))
      self.log.debug(status)
      raise CommandError(exitCode, stdout, stderr, status)
    finally:
      self.log.debug('stdout: ' + stdout.rstrip())
      self.log.debug('stderr: ' + stderr.rstrip())

  ######################################################################
  def _execute(self, stdin):
    """Execute the command once.

    Returns the output of the command.

    Arguments:
      stdin:  If not None, the stream from which the command should take its
              input.

    Exceptions:
      CommandError: if the command failed
    """
    command = self._getCommandLine() if self.shell else self._commandList
    try:
      p = Popen(command,
                stdin=PIPE, stdout=PIPE, stderr=PIPE, close_fds=True,
                env=self.env, shell=self.shell, universal_newlines=True)
      stdoutdata, stderrdata = p.communicate(stdin)
      return self._checkResults(p.returncode, "".join(stdoutdata),
                                "".join(stderrdata))
    except OSError as e:
      self._checkResults(e.errno, '',
                         ': '.join([self.commandName(), e.strerror]))
    except CommandError as e:
      error = e._stderr.split(os.linesep)[0]
      if error:
        e.setMessage(error)
      raise e

  ######################################################################
  def _getCommandLine(self):
    """Returns the appropriately quoted command line."""
    return ' '.join(self._commandList if self.shell
                    else map(pipes.quote, self._commandList))

########################################################################
def runCommand(commandList, **kwargs):
  """Run a command.

  Returns the output of the command (but see Keyword Arguments).

  Arguments:
    commandList: The command as a list of strings.

  Keyword Arguments:
    environment: A dict of environment variables and their values to use for
                 the command.
    noThrow:     If True, will return an empty string instead of throwing on
                 error.
    retries:     The number of times to try the command before giving up.
                 Defaults to 1.
    shell:       Indicate that this is a shell command.
    stdin:       If not None, the stream from which the command should take its
                 input, defaults to None.
    strip:       If True, strip leading and trailing whitespace from the
                 command output before returning it.

  Exceptions:
    CommandError: if the command failed and noThrow is False
  """
  return Command(commandList, kwargs.pop('environment', None)).run(**kwargs)

########################################################################
def tryCommandsUntilSuccess(commands, **kwargs):
  """Try each of a series of commands in turn until one succeeds. If all the
  commands fail, give up and raise an exception.

  Arguments:
    commands: A list of command lists

  Keyword Arguments:
    Supports all of the arguments which may be passed to runCommand().

  Returns:
    the output of the first successful command

  Exceptions:
    CommandError: if none of the commands succeeds and the noThrow keyword
                  option is False (or omitted); the error will be the one
                  raised by the last command in the list
  """
  error   = None
  noThrow = kwargs.pop('noThrow', False)
  for command in commands:
    try:
      return runCommand(command, **kwargs)
    except CommandError as e:
      error = e

  if noThrow:
    error = None

  # Pylint thinks we can raise None here.
  if error is not None:
    #pylint: disable=E0702
    raise error
