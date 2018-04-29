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

# $Id: //eng/vdo-releases/magnesium-rhel7.5/src/packaging/src-dist/user/utils/vdo/user/Makefile#1 $

VDO_VERSION = 6.1.0.168

UDS_DIR      = ../../uds
VDO_BASE_DIR = ../base

WARNS            =				\
		   -Wall			\
		   -Wcast-align			\
		   -Werror			\
		   -Wextra			\
		   -Winit-self			\
		   -Wlogical-op			\
		   -Wmissing-include-dirs	\
		   -Wpointer-arith		\
		   -Wredundant-decls		\
		   -Wunused			\
		   -Wwrite-strings		\

C_WARNS          =				\
		   -Wbad-function-cast		\
		   -Wcast-qual			\
		   -Wfloat-equal		\
		   -Wformat=2			\
		   -Wmissing-declarations	\
		   -Wmissing-format-attribute	\
		   -Wmissing-prototypes		\
		   -Wnested-externs		\
		   -Wold-style-definition	\
		   -Wswitch-default		\

OPT_FLAGS	 = -O3 -fno-omit-frame-pointer
DEBUG_FLAGS      =
GLOBAL_FLAGS     = -D_GNU_SOURCE -g $(OPT_FLAGS) $(WARNS)		\
		   $(shell getconf LFS_CFLAGS) -fpic $(DEBUG_FLAGS)
GLOBAL_CFLAGS	 = $(GLOBAL_FLAGS) -std=c99 $(C_WARNS) -pedantic	\
		   $(EXTRA_CFLAGS)
EXTRA_FLAGS      =
EXTRA_CFLAGS	 = $(EXTRA_FLAGS)
GLOBAL_LDFLAGS   = $(EXTRA_LDFLAGS)
EXTRA_LDFLAGS    =

DEPDIR           = .deps

MV               = mv -f

INCLUDES  = -I$(VDO_BASE_DIR) -I$(UDS_DIR)
CFLAGS 	  = -fPIC $(GLOBAL_CFLAGS) $(INCLUDES) -Wno-write-strings \
	    -DINTERNAL -DCURRENT_VERSION="\"$(VDO_VERSION)\""

LDFLAGS   = $(GLOBAL_LDFLAGS)
LDPRFLAGS = -ldl -pthread -lz -lrt -lm -luuid

DEPLIBS  = $(VDO_BASE_DIR)/libvdo.a $(UDS_DIR)/libuds.a
LIBFLAGS = -pthread -lrt

USER_OBJS   = blockMapUtils.o  \
              fileLayer.o      \
              parseUtils.o     \
              vdoConfig.o      \
              vdoVolumeUtils.o

# To add a new program X, add X to the variable PROGS (and possibly
# also DIST_PROGS).

PROGS = vdodumpconfig      \
        vdoforcerebuild    \
        vdoformat          \
        vdoprepareupgrade  \
        vdoreadonly

.PHONY: all
all: $(PROGS)
	$(MAKE) -C man all

.PHONY: clean
clean:
	$(MAKE) -C man clean
	rm -f *.a *.so *.o core*
	rm -fr $(DEPDIR) tmpdir
	rm -f  $(PROGS)

INSTALL = install
INSTALLOWNER ?= -o root -g root
bindir ?= /usr/bin
INSTALLDIR=$(DESTDIR)$(bindir)

.PHONY: install
install:
	$(INSTALL) $(INSTALLOWNER) -d $(INSTALLDIR)
	for i in $(PROGS); do \
	  $(INSTALL) $(INSTALLOWNER) -m 755 $$i $(INSTALLDIR); \
	done
	$(MAKE) -C man install

########################################################################
# Dependency processing

%.o: %.c
	$(COMPILE.c) -MMD -MF $(DEPDIR)/$*.d.new -MP -MT $@ -o $@ $<
	if cmp -s $(DEPDIR)/$*.d $(DEPDIR)/$*.d.new ;		 \
	then							 \
		$(RM) $(DEPDIR)/$*.d.new ;			 \
	else							 \
		$(MV) $(DEPDIR)/$*.d.new $(DEPDIR)/$*.d ;	 \
	fi

$(DEPDIR)/%.d: %.c
	@mkdir -p $(DEPDIR)
	$(CC) $(CFLAGS) -MM -MF $@ -MP -MT $*.o $<

.SECONDEXPANSION:
$(PROGS): $$@.o $(USER_OBJS) $(DEPLIBS)
	echo "Building $@ from $^"
	$(CC) $(LDFLAGS) $^ $(LDPRFLAGS) -o $@

ifneq ($(MAKECMDGOALS),clean)
DEPSOURCES = $(wildcard *.c)
-include $(DEPSOURCES:%.c=$(DEPDIR)/%.d)
endif
