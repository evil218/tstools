#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

VMAJOR = 1
VMINOR = 0
VRELEA = 0

obj-y := if.o
obj-y += udp.o
obj-y += url.o
obj-y += UTF_GB.o

NAME = zutil
TYPE = lib
DESC = common functions
HEADERS = common.h if.h udp.h url.h G2U.h U2G.h UTF_GB.h

ifeq ($(SYS),WINDOWS)
LDFLAGS += -lws2_32
endif

include ../common.mak
