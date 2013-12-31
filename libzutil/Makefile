#
# Makefile
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := if.o
obj-y += udp.o
obj-y += url.o

VMAJOR = 1
VMINOR = 1
VPATCH = 0
NAME = zutil
TYPE = lib
DESC = common functions
HEADERS = common.h if.h udp.h url.h
INCDIRS := -I. -I..

CFLAGS += $(INCDIRS)

ifeq ($(SYS),WINDOWS)
LDFLAGS += -lws2_32
endif

LINTFLAGS := +posixlib

include ../common.mak
