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

BUILD_VERSION = 8.3.0.73

DEPDIR = .deps

ifdef LLVM
	export CC := clang
	export LD := ld.ldd
endif

ifeq ($(origin CC), default)
	CC := gcc
endif

ifeq ($(findstring clang, $(CC)),clang)
# Ignore additional warnings for clang
	WARNS =	-Wno-compare-distinct-pointer-types	 \
		-Wno-gnu-statement-expression		 \
		-Wno-gnu-zero-variadic-macro-arguments	 \
		-Wno-implicit-const-int-float-conversion \
		-Wno-language-extension-token
else
	WARNS =	-Wcast-align	\
		-Wcast-qual	\
		-Wformat=2	\
		-Wlogical-op
endif

WARNS        +=	-Wall			\
		-Werror			\
		-Wextra			\
		-Winit-self		\
		-Wmissing-include-dirs	\
		-Wpointer-arith		\
		-Wredundant-decls	\
		-Wunused		\
		-Wwrite-strings

C_WARNS       =	-Wbad-function-cast		\
		-Wfloat-equal			\
		-Wmissing-declarations		\
		-Wmissing-format-attribute	\
		-Wmissing-prototypes		\
		-Wnested-externs		\
		-Wold-style-definition		\
		-Wswitch-default

OPT_FLAGS      = -O3 -fno-omit-frame-pointer
DEBUG_FLAGS    =
RPM_OPT_FLAGS ?= -fpic
GLOBAL_FLAGS   = $(RPM_OPT_FLAGS) -D_GNU_SOURCE -g $(OPT_FLAGS)		\
		 $(WARNS) $(shell getconf LFS_CFLAGS) $(DEBUG_FLAGS)	\
		 -DCURRENT_VERSION='"$(BUILD_VERSION)"'			\

CFLAGS  = $(GLOBAL_FLAGS) -I. -std=gnu11 -pedantic $(C_WARNS) $(MY_CFLAGS)
LDFLAGS = $(RPM_LD_FLAGS) $(MY_LDFLAGS)

MY_FLAGS    =
MY_CFLAGS   = $(MY_FLAGS)
MY_LDFLAGS  =

vpath %.c .

UDS_OBJECTS =	chapter-index.o		\
		config.o		\
		delta-index.o		\
		dm-bufio.o              \
		errors.o		\
		event-count.o		\
		fileUtils.o		\
		funnel-queue.o		\
		geometry.o		\
		index.o			\
		index-layout.o		\
		index-page-map.o	\
		index-session.o		\
		io-factory.o		\
		logger.o		\
		memoryAlloc.o		\
		minisyslog.o		\
		murmurhash3.o		\
		open-chapter.o		\
		permassert.o		\
		radix-sort.o		\
		random.o		\
		requestQueue.o		\
		sparse-cache.o		\
		string-utils.o		\
		syscalls.o		\
		threadCondVar.o		\
		threadMutex.o		\
		threadSemaphore.o	\
		thread-utils.o		\
		time-utils.o		\
		volume.o		\
		volume-index.o

.PHONY: all
all: libuds.a

.PHONY: clean
clean:
	rm -rf *.o *.a $(DEPDIR)

.PHONY: install
install:;

libuds.a: $(UDS_OBJECTS)
	rm -f $@
	ar cr $@ $^

%.s: %.c
	$(CC) $(CFLAGS) -S $^


########################################################################
# Dependency processing

%.o: %.c
	@mkdir -p $(DEPDIR)/$(@D) $(@D)
	$(COMPILE.c) -MD -MF $(DEPDIR)/$*.d.new -MP -MT $@ $< -o $@
	if cmp -s $(DEPDIR)/$*.d $(DEPDIR)/$*.d.new; then \
		rm -f $(DEPDIR)/$*.d.new ; \
	else \
		mv -f $(DEPDIR)/$*.d.new $(DEPDIR)/$*.d ; \
	fi

$(DEPDIR)/%.d: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MM -MF $@ -MP -MT $*.o $<

ifneq ($(MAKECMDGOALS),clean)
-include $(UDS_OBJECTS:%.o=$(DEPDIR)/%.d)
endif
