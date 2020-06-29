#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

ifneq (,$(wildcard $(NRFJPROG_PATH)/nrfjprogdll.h))
    NRFJPROG_REAL_PATH=$(NRFJPROG_PATH)
else ifneq (,$(wildcard $(NRFJPROG_PATH)/nrfjprog/nrfjprogdll.h))
    NRFJPROG_REAL_PATH=$(NRFJPROG_PATH)/nrfjprog
else
    NRFJPROG_EXE_PATH=$(shell which nrfjprog)
    ifneq (,$(NRFJPROG_EXE_PATH))
        NRFJPROG_REAL_PATH=$(dir $(NRFJPROG_EXE_PATH))
    endif
endif

NRFJPROG_REAL_PATH := $(NRFJPROG_REAL_PATH:/=)
NRFJPROG_REAL_PATH := $(NRFJPROG_REAL_PATH:/=)

ifeq (,$(NRFJPROG_REAL_PATH))
    $(info Directory containing nrfjprog must be in your PATH variable or)
    $(info NRFJPROG_PATH pointing that directory must be provided.)
    $(error Cannot find nrfjprog directory)
endif

ifeq (1,$(DEBUG))
    CFLAGS=-g -O0
    STRIP=echo Skipping strip
else
    CFLAGS=-O3
    STRIP=strip
endif

CFLAGS+= -I. -ISEGGER -IConfig -m32 -DDO_TESTING=1

-include version.make

all: SysViewLight

clean:
	rm -f SysViewLight

#SysViewLight: Makefile version.make ../SysView/*.c ../SysView/*.h ./SEGGER/SEGGER_RTT.c ./SEGGER/SEGGER_SYSVIEW.c
SysViewLight: Makefile version.make ../SysView/main.cpp
	g++ $(CFLAGS) -o $@ -I$(NRFJPROG_REAL_PATH) $(filter %.cpp,$^) $(filter %.c,$^) -ldl
	$(STRIP) $@

version.make: get_version.sh $(wildcard .git/HEAD) $(wildcard .git/refs/tags/*)
	bash get_version.sh
