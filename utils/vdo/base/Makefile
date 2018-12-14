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
# $Id: //eng/vdo-releases/aluminum/src/packaging/src-dist/user/utils/vdo/base/Makefile#2 $

VDO_VERSION = 6.2.0.293

UDS_DIR      = ../../uds


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
RPM_OPT_FLAGS   ?= -fpic
GLOBAL_FLAGS     = $(RPM_OPT_FLAGS) -D_GNU_SOURCE -g $(OPT_FLAGS) $(WARNS) \
		   $(shell getconf LFS_CFLAGS) $(DEBUG_FLAGS)
GLOBAL_CFLAGS	 = $(GLOBAL_FLAGS) -std=c99 $(C_WARNS) -pedantic	\
		   $(EXTRA_CFLAGS)
EXTRA_FLAGS      =
EXTRA_CFLAGS	 = $(EXTRA_FLAGS)
GLOBAL_LDFLAGS   = $(RPM_LD_FLAGS) $(EXTRA_LDFLAGS)
EXTRA_LDFLAGS    =

DEPDIR           = .deps

MV               = mv -f

INCLUDES = -I. -I$(UDS_DIR)
CFLAGS   = -fPIC $(GLOBAL_CFLAGS) $(INCLUDES) -Wno-write-strings \
	   -DCURRENT_VERSION="\"$(VDO_VERSION)\""
LDFLAGS  = $(GLOBAL_LDFLAGS)

C_FILES := $(wildcard *.c)
OBJECTS := $(C_FILES:%.c=%.o)

.PHONY: all
all: libvdo.a

.PHONY: clean
clean:
	rm -f *.o *.a
	rm -rf $(DEPDIR)

.PHONY: install
install:;

libvdo.a: $(OBJECTS)
	echo $$(OBJECTS)
	$(RM) $@
	$(AR) cr $@ $(OBJECTS)

# Suppress a pedantic warning, which we can't fix without some serious
# divergence from upstream: The lz4 package freely casts const pointer
# values to non-const pointer values.
lz4.o: CFLAGS:= $(filter-out -Wcast-qual,$(CFLAGS))

%.o: %.c
	$(COMPILE.c) -MMD -MF $(DEPDIR)/$*.d.new -MP -MT $@ -o $@ $<
	if cmp -s $(DEPDIR)/$*.d $(DEPDIR)/$*.d.new ;		\
	then							\
		$(RM) $(DEPDIR)/$*.d.new ;			\
	else							\
		$(MV) $(DEPDIR)/$*.d.new $(DEPDIR)/$*.d ;	\
	fi

$(DEPDIR)/%.d: %.c
	@mkdir -p $(DEPDIR)
	$(CC) $(CFLAGS) -MM -MF $@ -MP -MT $*.o $<

ifneq ($(MAKECMDGOALS),clean)
DEPSOURCES = $(wildcard *.c)
-include $(DEPSOURCES:%.c=$(DEPDIR)/%.d)
endif
