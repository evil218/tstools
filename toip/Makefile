#
# Makefile
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := toip.o

VMAJOR = 1
VMINOR = 0
VRELEA = 0
NAME = toip
TYPE = exe
INCDIRS := -I. -I..
INCDIRS += -I../libzutil
INCDIRS += -I../libzts
INCDIRS += -I../libzlst
CFLAGS += $(INCDIRS)

LDFLAGS += -L../libzutil -lzutil
LDFLAGS += -L../libzts -lzts

include ../common.mak
