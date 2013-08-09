#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := catip.o

VMAJOR = 1
VMINOR = 0
VRELEA = 0
NAME = catip
TYPE = exe
INCDIRS := -I. -I..
INCDIRS += -I../libzutil
CFLAGS += $(INCDIRS)

LDFLAGS += -L../libzutil -lzutil

include ../common.mak
