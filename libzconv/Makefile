#
# Makefile
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := zconv.o

VMAJOR = 1
VMINOR = 0
VRELEA = 0
NAME = zconv
TYPE = lib
DESC = DVB charsets convert
HEADERS = zconv.h
INCDIRS := -I. -I..

CFLAGS += $(INCDIRS)

ifeq ($(SYS),WINDOWS)
LDFLAGS += -lws2_32
endif

LINTFLAGS := +posixlib

include ../common.mak
