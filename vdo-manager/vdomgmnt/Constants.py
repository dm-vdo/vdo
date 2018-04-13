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
  Constants - manage VDO constants.

  $Id: //eng/vdo-releases/aluminum/src/python/vdo/vdomgmnt/Constants.py#1 $

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
import gettext
gettext.install('vdomgmnt')

class Constants(object):
  """Constants manages constant values."""
  LOCK_DIR       = '/var/lock/vdo'
  SECTOR_SIZE    = 512
  VDO_BLOCK_SIZE = 4096
  (KB, MB, GB, TB, PB, EB) = map(lambda x: 2 ** (10 * x), range(1, 7))

  lvmByteSuffix = 'b'
  lvmKiloSuffix = 'k'
  lvmMegaSuffix = 'm'
  lvmGigaSuffix = 'g'
  lvmTeraSuffix = 't'
  lvmPetaSuffix = 'p'
  lvmExaSuffix = 'e'

  lvmSectorSuffix = 's'

  lvmDefaultSuffix = lvmMegaSuffix
  lvmDefaultUnitsText = "megabytes"

  # The SI suffixes must be ordered least to greatest.
  lvmSiSuffixes = [lvmByteSuffix, lvmKiloSuffix, lvmMegaSuffix, lvmGigaSuffix,
                   lvmTeraSuffix, lvmPetaSuffix, lvmExaSuffix]
  lvmSiSuffixSizeMap = dict(zip(lvmSiSuffixes, [1, KB, MB, GB, TB, PB, EB]))
  lvmSiSuffixTextMap = dict(zip(lvmSiSuffixes,
                                [_("B(ytes)"),
                                 _("K(ilobytes)"),
                                 _("M(egabytes)"),
                                 _("G(igabytes)"),
                                 _("T(erabytes)"),
                                 _("P(etabytes)"),
                                 _("E(xabytes)")]))

  lvmSuffixes = [lvmSectorSuffix] + lvmSiSuffixes
  lvmSuffixSizeMap = { lvmSectorSuffix : SECTOR_SIZE }
  lvmSuffixSizeMap.update(lvmSiSuffixSizeMap)
  lvmSuffixTextMap = { lvmSectorSuffix : _("S(ectors)") }
  lvmSuffixTextMap.update(lvmSiSuffixTextMap)

  disabled = 'disabled'
  enabled = 'enabled'
  enableChoices = [disabled, enabled]

  DEDUPLICATION_TIMEOUT = 20
  deduplicationStatusError = 'error'
  deduplicationStatusOnline = 'online'
  deduplicationStatusOpening = 'opening'

  class dmsetupStatusFields():
    sectorCount = 1
    storageDevice = 3
    modeState = 4
    inRecovery = 5
    deduplicationStatus = 6
    compressionStatus = 7
 
  @classmethod
  def enableString(cls, value):
    return cls.enabled if value else cls.disabled
