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
# $Id: //eng/vdo-releases/aluminum/src/packaging/src-dist/user/examples/systemd/Makefile#1 $

SERVICEFILE=vdo.service
EXAMPLEFILES=VDO.mount.example
PRESETFILE=97-vdo.preset

INSTALL = install
INSTALLOWNER ?= -o root -g root
unitdir ?= /lib/systemd/system
presetdir ?= /lib/systemd/system-preset
defaultdocdir ?= /usr/share/doc
name ?= vdo

INSTALLDIR=$(DESTDIR)/$(unitdir)
EXAMPLESDIR=$(DESTDIR)/$(defaultdocdir)/$(name)/examples/systemd
PRESETDIR=$(DESTDIR)/$(presetdir)

.PHONY: all clean install
all:;

clean:;

install:
	$(INSTALL) $(INSTALLOWNER) -d $(INSTALLDIR)
	$(INSTALL) $(INSTALLOWNER) -m 644 $(SERVICEFILE) $(INSTALLDIR)
	$(INSTALL) $(INSTALLOWNER) -d $(PRESETDIR)
	$(INSTALL) $(INSTALLOWNER) -m 644 $(PRESETFILE) $(PRESETDIR)
	$(INSTALL) -d $(EXAMPLESDIR)
	for i in $(EXAMPLEFILES); do \
	  $(INSTALL) $(INSTALLOWNER) -m 644 $$i $(EXAMPLESDIR); \
	done
