#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := if.o
obj-y += udp.o
obj-y += url.o
obj-y += UTF_GB.o

VMAJOR = 1
VMINOR = 0
VRELEA = 0
NAME = zutil
TYPE = lib
DESC = common functions
HEADERS = common.h if.h udp.h url.h G2U.h U2G.h UTF_GB.h
INCDIRS := -I. -I..

CFLAGS += $(INCDIRS)

ifeq ($(SYS),WINDOWS)
LDFLAGS += -lws2_32
endif

include ../common.mak
