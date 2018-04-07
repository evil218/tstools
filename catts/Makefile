#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := catts.o

VMAJOR = 1
VMINOR = 0
VRELEA = 0
NAME = catts
TYPE = exe
INCDIRS := -I. -I..
INCDIRS += -I../libzutil
CFLAGS += $(INCDIRS)

LDFLAGS += -L../libzutil -lzutil

include ../common.mak
