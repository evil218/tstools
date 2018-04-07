#
# Makefile
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := dtsdi.o

VMAJOR = 1
VMINOR = 0
VRELEA = 0
NAME = dtsdi
TYPE = exe
INCDIRS := -I. -I..
INCDIRS += -I../libzutil
CFLAGS += $(INCDIRS)

LDFLAGS += -L../libzutil -lzutil

include ../common.mak
