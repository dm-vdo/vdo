#
# Copyright Red Hat
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

INSTALLFILES= \
	adaptlvm.8         \
	vdoaudit.8         \
	vdodebugmetadata.8 \
	vdodumpblockmap.8  \
	vdodumpmetadata.8  \
	vdoforcerebuild.8  \
	vdoformat.8        \
	vdolistmetadata.8  \
	vdoreadonly.8      \
	vdorecover.8       \
	vdostats.8

INSTALL = install
INSTALLOWNER ?= -o root -g root
mandir ?= /usr/man

INSTALLDIR=$(DESTDIR)/$(mandir)

.PHONY: all clean install
all:;

clean:;

install:
	$(INSTALL) $(INSTALLOWNER) -d $(INSTALLDIR)/man8
	for i in $(INSTALLFILES); do		\
	  $(INSTALL) $(INSTALLOWNER) -m 644 $$i $(INSTALLDIR)/man8;	\
	done
