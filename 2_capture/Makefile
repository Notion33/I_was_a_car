# Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an
# express license agreement from NVIDIA Corporation is strictly prohibited.

# edited by Hyundai Autron

CC = gcc

TARGETS = nvmedia_capture

CFLAGS   = -Wall -I. -I./utils
CPPFLAGS = -I./include

OBJS := nvmedia_capture.o
OBJS += nvthread.o

LDLIBS :=
LDLIBS += -L ./utils
LDLIBS += -lnvmedia
LDLIBS += -lnvtestutil_board
LDLIBS += -lnvtestutil_capture_input
LDLIBS += -lnvtestutil_i2c
LDLIBS += -lpthread

default : $(TARGETS)

$(TARGETS): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clobber clean :
	rm -rf $(TARGETS) $(OBJS)

