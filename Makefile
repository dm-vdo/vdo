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

INSTALL = install
INSTALLOWNER ?= -o root -g root
name ?= vdo
defaultdocdir ?= /usr/share/doc
defaultlicensedir ?= /usr/share/licenses
DOCDIR=$(DESTDIR)/$(defaultdocdir)/$(name)
LICENSEDIR=$(DESTDIR)/$(defaultlicensedir)/$(name)

SUBDIRS = examples utils

.PHONY: all clean install
all clean:
	for d in $(SUBDIRS); do         \
	  $(MAKE) -C $$d $@ || exit 1; \
	done

install:
	$(INSTALL) $(INSTALLOWNER) -d $(DOCDIR)
	$(INSTALL) $(INSTALLOWNER) -D -m 644 COPYING -t $(LICENSEDIR) 
	for d in $(SUBDIRS); do         \
	  $(MAKE) -C $$d $@ || exit 1; \
	done
