#
# Copyright (c) 2017 Red Hat, Inc.
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
# $Id: //eng/vdo-releases/magnesium/src/packaging/src-dist/user/vdo-manager/utils/Makefile#1 $

PYTHON_MODULES  = $(wildcard *.py)

INSTALLDIR=$(DESTDIR)/$(python_sitelib)/$(name)/utils

.PHONY: all clean install

all:;

install:
	install -d $(INSTALLDIR)
	for i in $(PYTHON_MODULES); do	\
	  install $$i $(INSTALLDIR);	\
	done

clean:
	-rm -f *.pyc
