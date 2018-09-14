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
# $Id: //eng/vdo-releases/magnesium-rhel7.6/src/packaging/src-dist/user/vdo-manager/Makefile#1 $

SUBDIRS=man statistics utils vdomgmnt

PROGS=vdo vdostats

INSTALL = install
INSTALLOWNER ?= -o root -g root
bindir ?= /usr/bin
INSTALLDIR=$(DESTDIR)$(bindir)

.PHONY: all clean install
all clean:
	set -e;					\
	for d in $(SUBDIRS); do			\
	  $(MAKE) -C $$d $@ || exit 1;		\
	done

install:
	$(INSTALL) $(INSTALLOWNER) -d $(INSTALLDIR)
	set -e;						\
	for p in $(PROGS); do				\
	  $(INSTALL) $(INSTALLOWNER) $$p $(INSTALLDIR);	\
	done
	set -e;						\
	for d in $(SUBDIRS); do				\
	  $(MAKE) -C $$d install || exit 1;		\
	done
