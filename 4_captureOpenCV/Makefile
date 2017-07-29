# Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an
# express license agreement from NVIDIA Corporation is strictly prohibited.

# edited by Hyundai Autron

CC = gcc

TARGETS = captureOpenCV

OPT = -Os -O2
CFLAGS   = $(OPT) -Wall -I. -I./utils `pkg-config opencv --cflags`
CPPFLAGS = -I./include

OBJS := captureOpenCV.o
OBJS += nvthread.o

LDLIBS :=
LDLIBS += -L ./utils
LDLIBS += -lnvmedia
LDLIBS += -lnvtestutil_board
LDLIBS += -lnvtestutil_capture_input
LDLIBS += -lnvtestutil_i2c
LDLIBS += -lpthread
LDLIBS += `pkg-config opencv --libs`

default : $(TARGETS)

$(TARGETS): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clobber clean :
	rm -rf $(TARGETS) $(OBJS)

