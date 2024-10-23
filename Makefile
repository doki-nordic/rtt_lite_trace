#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

ifeq (1,$(DEBUG))
    CFLAGS=-g -O0
    STRIP=echo Skipping strip
else
    CFLAGS=-O3
    STRIP=strip
endif

CFLAGS+= -I. -ISEGGER -IConfig -m32

-include version.make

all: SysViewLight

clean:
	rm -f SysViewLight

SysViewLight: Makefile version.make main.cpp ./*.h ./SEGGER/SEGGER_RTT.c ./SEGGER/SEGGER_SYSVIEW.c
	g++ $(CFLAGS) -o $@ $(filter %.cpp,$^) $(filter %.c,$^)
	$(STRIP) $@

version.make: get_version.sh $(wildcard .git/HEAD) $(wildcard .git/refs/tags/*)
	bash get_version.sh
