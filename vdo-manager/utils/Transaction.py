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
  Transaction - provides transactional support

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/utils/Transaction.py#1 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from functools import wraps
import gettext
import sys
import threading

gettext.install('Transaction')

#####################################################################
if (sys.version_info > (3, 0)):
  def reraise(ex, val, tb):
    #pylint: disable=E0710
    raise ex.with_traceback(tb)
else:
  exec("""def reraise(ex, val, tb):
    raise type(ex), val, tb
    """)


class Transaction(object):
  """Client-visible transaction object.
  """

  # A dictionary mapping thread identifier to a list of the thread's
  # transactions.
  __transactionLists = {}

  ######################################################################
  # Public methods
  ######################################################################
  @classmethod
  def transaction(cls):
    """Returns the current transaction for the current thread.
    """
    return cls._threadTransactionList()[-1]

  ######################################################################
  def addUndoStage(self, undoStage):
    """Adds callable undoStage to transaction stages.

       Arguments:
        undoStage (callable)        - callable to execute as part of
                                      transaction roll-back
    """
    self.__undoStages.append(undoStage)

  ######################################################################
  def setMessage(self, handleMessage, message = None):
    """Sets the message to handle if there is an exception.

       If message is specified and an exception occurs the exception
       will be appended to the message using "; {0}" where "{0}" represents
       the exception.

       If message is not specified, the exception will be coerced to
       a string and passed to handleMessage.

       Specifying handleMessage as None will clear both handleMessage and
       message.

       Arguments:
        handleMessage (callable)  - method to handle message
        message (string)          - message
    """
    raise NotImplementedError

  ######################################################################
  # Overridden methods
  ######################################################################
  def __init__(self):
    super(Transaction, self).__init__()
    self.__undoStages = []

  ######################################################################
  # Protected methods
  ######################################################################
  @classmethod
  def _threadTransactionList(cls):
    """Returns the transaction list for the current thread, creating it
       if need be.

       Returns:
         list of Transactions
    """
    transactionList = None
    threadId = threading.currentThread().ident
    try:
      transactionList = cls.__transactionLists[threadId]
    except KeyError:
      transactionList = []
      cls.__transactionLists[threadId] = transactionList
    return transactionList

  ######################################################################
  def _undoStages(self):
    """Returns the list of undo stages for the transaction.

       Returns:
         list of undo stages
    """
    return self.__undoStages

  ######################################################################
  # Private methods
  ######################################################################

########################################################################
def transactional(func):
  """Method decorator providing transactional capabilities to the method.
  """

  ######################################################################
  class _Transaction(Transaction):
    """Decorator-local transaction object providing actual transaction
       capabilities.
    """

    ####################################################################
    # Public methods
    ####################################################################
    @classmethod
    def addTransaction(cls):
      """Adds, and returns, a transaction to the transaction list for the
         current thread.
      """
      cls._threadTransactionList().append(cls())
      return cls.transaction()

    ####################################################################
    @classmethod
    def removeTransaction(cls):
      """Removes the current transaction for for the current thread.
      """
      cls._threadTransactionList().pop()

    ####################################################################
    def undo(self, exception):
      """Performs the undo processing of the transaction.

         Exceptions from the undo stages are ignored.

         Arguments:
          exception   - the exception which resulted in undo being called
      """
      # Handle any message that was set for the transaction.
      if self.__handleMessage is not None:
        if self.__message is not None:
          self.__handleMessage(_("{0}; {1}").format(self.__message,
                                                     str(exception)))
        else:
          self.__handleMessage(str(exception))

      # Perform the undo processing.
      undoStages = self._undoStages()[:]
      undoStages.reverse()
      for undoStage in undoStages:
        try:
          undoStage()
        except Exception:
          pass

    ####################################################################
    # Overridden methods
    ####################################################################
    def __init__(self):
      super(_Transaction, self).__init__()
      self.__handleMessage = None
      self.__message = None

    ####################################################################
    def setMessage(self, handleMessage, message = None):
      self.__handleMessage = handleMessage
      if self.__handleMessage is None:
        self.__message = None
      else:
        self.__message = message

    ####################################################################
    # Protected methods
    ####################################################################

    ####################################################################
    # Private methods
    ####################################################################

  ######################################################################
  @wraps(func)
  def wrap(*args, **kwargs):
    """Wrapper method providing transactional processing capabilities.
    """
    transaction = _Transaction.addTransaction()

    result = None
    try:
      result = func(*args, **kwargs)
    except Exception as ex:
      transaction.undo(ex)
      reraise(ex, ex, sys.exc_info()[2])
    finally:
      _Transaction.removeTransaction()

    return result
  return wrap
