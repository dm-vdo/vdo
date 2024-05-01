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

VDO_VERSION = 8.4.0.0

UDS_DIR      = ../uds


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

ifeq ($(AR), ar)
	ifeq ($(origin AR), default)
		AR := gcc-ar
	endif
endif

OPT_FLAGS	 = -O3 -fno-omit-frame-pointer
DEBUG_FLAGS      =
RPM_OPT_FLAGS   ?= -fpic
GLOBAL_FLAGS     = $(RPM_OPT_FLAGS) -D_GNU_SOURCE -g $(OPT_FLAGS) $(WARNS) \
		   $(shell getconf LFS_CFLAGS) $(DEBUG_FLAGS)
GLOBAL_CFLAGS	 = $(GLOBAL_FLAGS) -std=gnu11 -pedantic $(C_WARNS)	\
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

LDFLAGS   = $(GLOBAL_LDFLAGS)
LDPRFLAGS = -ldl -pthread -lz -lrt -lm -luuid

DEPLIBS  = $(UDS_DIR)/libuds.a
LIBFLAGS = -pthread -lrt

PROGS = vdoaudit               \
        vdodebugmetadata       \
        vdodmeventd            \
        vdodumpblockmap        \
        vdodumpconfig          \
        vdodumpmetadata        \
        vdoforcerebuild        \
        vdoformat              \
        vdolistmetadata        \
        vdoreadonly            \
        vdoregenerategeometry  \
        vdosetuuid             \
        vdostats

COMPLETIONS=vdostats

NOBUILDPROGS = adaptlvm \
	       vdorecover
PROG_SOURCES := $(PROGS:%=%.c)
C_FILES      := $(filter-out $(PROG_SOURCES),$(wildcard *.c))
LIB_OBJECTS  := $(C_FILES:%.c=%.o)

DMEVENT_LIB = libdevmapper-event-lvm2vdo.so

.PHONY: all
all: libvdo.a $(PROGS)

.PHONY: clean
clean:
	$(MAKE) -C man clean
	rm -f *.o *.a
	rm -rf $(DEPDIR) $(PROGS)

libvdo.a: $(LIB_OBJECTS)
	$(RM) $@
	$(AR) cr $@ $(LIB_OBJECTS)

INSTALL = install
INSTALLOWNER ?= -o root -g root
bindir ?= /usr/bin
INSTALLDIR=$(DESTDIR)$(bindir)
bash_completions_dir ?= /usr/share/bash-completion/completions
COMPLETIONINSTALLDIR=$(DESTDIR)$(bash_completions_dir)/

.PHONY: install
install:
	$(INSTALL) $(INSTALLOWNER) -d $(INSTALLDIR)
	for i in $(PROGS) $(NOBUILDPROGS); do \
	  $(INSTALL) $(INSTALLOWNER) -m 755 $$i $(INSTALLDIR); \
	done
	$(MAKE) -C man install
	$(INSTALL) $(INSTALLOWNER) -d $(COMPLETIONINSTALLDIR)
	for c in $(COMPLETIONS); do                            \
	  $(INSTALL) $(INSTALLOWNER) -m 644 $$c.bash           \
	         $(COMPLETIONINSTALLDIR)/$$c;                  \
	done

########################################################################
# Dependency processing

%.o: %.c
	$(COMPILE.c) -MMD -MF $(DEPDIR)/$*.d.new -MP -MT $@ -o $@ $<
	if cmp -s $(DEPDIR)/$*.d $(DEPDIR)/$*.d.new ;		\
	then							\
		$(RM) $(DEPDIR)/$*.d.new ;			\
	else							\
		$(MV) $(DEPDIR)/$*.d.new $(DEPDIR)/$*.d ;	\
	fi

vdodmeventd.o: CFLAGS += "-DPLUGIN_NAME=\"$(DMEVENT_LIB)\""

$(DEPDIR)/%.d: %.c
	@mkdir -p $(DEPDIR)
	$(CC) $(CFLAGS) -MM -MF $@ -MP -MT $*.o $<

.SECONDEXPANSION:
$(PROGS): $$@.o libvdo.a $(DEPLIBS)
	echo "Building $@ from $^"
	$(CC) $(LDFLAGS) $^ $(LDPRFLAGS) -o $@

vdoformat: LDPRFLAGS += "-lblkid"
vdodmeventd: LDPRFLAGS += "-ldevmapper-event"

ifneq ($(MAKECMDGOALS),clean)
DEPSOURCES = $(wildcard *.c)
-include $(DEPSOURCES:%.c=$(DEPDIR)/%.d)
endif
