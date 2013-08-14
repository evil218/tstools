#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := tsana.o

VMAJOR = 1
VMINOR = 0
VRELEA = 0
NAME = tsana
TYPE = exe
INCDIRS := -I. -I..
INCDIRS += -I../libzutil
INCDIRS += -I../libzbuddy
INCDIRS += -I../libzts
INCDIRS += -I../libzlst
INCDIRS += -I../libparam_xml
INCDIRS += -I/usr/include/libxml2
CFLAGS += $(INCDIRS)

LDFLAGS += -L../libzutil -lzutil
LDFLAGS += -L../libzbuddy -lzbuddy
LDFLAGS += -L../libzlst -lzlst
LDFLAGS += -L../libzts -lzts
LDFLAGS += -L../libparam_xml -lparam_xml

ifeq ($(ARCH),X86_64)
LDFLAGS += -L/usr/lib/x86_64-linux-gnu -lxml2
else
LDFLAGS += -L/usr/lib -lxml2
endif

LINTFLAGS := +posixlib

include ../common.mak
