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
# $Id: //eng/vdo-releases/chlorine/src/packaging/src-dist/user/Makefile#1 $

INSTALL = install
INSTALLOWNER ?= -o root -g root
defaultdocdir ?= /usr/share/doc
licensedir ?= $(defaultdocdir)
name ?= vdo
LICENSEDIR=$(DESTDIR)/$(licensedir)/$(name)
INSTALLDIR=$(DESTDIR)/$(defaultdocdir)/$(name)

SUBDIRS = examples utils

.PHONY: all clean install
all clean:
	for d in $(SUBDIRS); do         \
	  $(MAKE) -C $$d $@ || exit 1; \
	done

install:
	$(INSTALL) $(INSTALLOWNER) -d $(INSTALLDIR)
	$(INSTALL) $(INSTALLOWNER) -D -m 644 COPYING -t $(LICENSEDIR) 
	for d in $(SUBDIRS); do         \
	  $(MAKE) -C $$d $@ || exit 1; \
	done
