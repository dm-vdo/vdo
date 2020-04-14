#
# Copyright (c) 2020 Red Hat, Inc.
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
# $Id: //eng/linux-vdo/src/packaging/src-dist/user/utils/uds/Makefile#16 $

# UDS_INTERNAL_VERSION is the short version name, which is used to version the
# UDS interfaces exported from libuds.so.  DISTRIBUTION_VERSION is the long
# version name used in distribution builds.  We extract these values from the
# traditional location.
UDS_VERSION      = 8.1.0.25
BUILD_VERSION    = $(UDS_VERSION)
DISTRO_CODENAME := $(shell lsb_release -s -c)

DEPDIR  = .deps

ifeq ($(origin CC), default)
  CC=gcc
endif

WARNS =	-Wall			\
	-Wcast-align		\
	-Werror			\
	-Wextra			\
	-Winit-self		\
	-Wlogical-op		\
	-Wmissing-include-dirs	\
	-Wpointer-arith		\
	-Wredundant-decls	\
	-Wunused		\
	-Wwrite-strings

C_WARNS =	-Wbad-function-cast		\
		-Wcast-qual			\
		-Wfloat-equal			\
		-Wformat=2			\
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
		 -DUDS_VERSION='"$(BUILD_VERSION)"'			\

CFLAGS  = $(GLOBAL_FLAGS) -I. -std=c99 $(C_WARNS) -pedantic $(MY_CFLAGS)
LDFLAGS = $(RPM_LD_FLAGS) $(MY_LDFLAGS)

MY_FLAGS    =
MY_CFLAGS   = $(MY_FLAGS)
MY_LDFLAGS  =

IMPORTED_SYMBOLS = libuds.imported

vpath %.c .
vpath %.c ./murmur
vpath %.c ./util

UDS_OBJECTS =	MurmurHash3.o			\
		bits.o				\
		buffer.o			\
		bufferedReader.o		\
		bufferedWriter.o		\
		cacheCounters.o			\
		cachedChapterIndex.o		\
		chapterIndex.o			\
		chapterWriter.o			\
		config.o			\
		deltaIndex.o			\
		deltaMemory.o			\
		errors.o			\
		eventCount.o			\
		fileIORegion.o			\
		fileUtils.o			\
		funnelQueue.o			\
		geometry.o			\
		hashUtils.o			\
		index.o				\
		indexCheckpoint.o		\
		indexComponent.o		\
		indexConfig.o			\
		indexInternals.o		\
		indexLayout.o			\
		indexLayoutLinuxUser.o		\
		indexLayoutParser.o		\
		indexPageMap.o			\
		indexRouter.o			\
		indexSession.o			\
		indexState.o			\
		indexStateData.o		\
		indexVersion.o			\
		indexZone.o			\
		ioFactoryLinuxUser.o		\
		loadType.o			\
		logger.o			\
		loggerLinuxUser.o		\
		masterIndex005.o		\
		masterIndex006.o		\
		masterIndexOps.o		\
		memoryAlloc.o			\
		memoryLinuxUser.o		\
		minisyslog.o			\
		nonce.o				\
		openChapter.o			\
		openChapterZone.o		\
		pageCache.o			\
		permassert.o			\
		permassertLinuxUser.o		\
		radixSort.o			\
		random.o			\
		recordPage.o			\
		request.o			\
		requestQueueUser.o		\
		searchList.o			\
		sparseCache.o			\
		stringLinuxUser.o		\
		stringUtils.o			\
		syscalls.o			\
		threadCondVarLinuxUser.o	\
		threadsLinuxUser.o		\
		threadMutexLinuxUser.o		\
		threadSemaphoreLinuxUser.o	\
		threadOnce.o			\
		timeUtils.o			\
		udsMain.o			\
		volume.o			\
		volumeStore.o			\
		zone.o

.PHONY: all
all: libuds.a

.PHONY: clean
clean:
	rm -f *.o *.a *.s *.so lib*.so.* wrapped-symbols
	rm -rf doxygen $(DEPDIR)

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
