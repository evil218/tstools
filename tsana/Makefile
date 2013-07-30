#
# Makefile for libzlst
#

include ../config.mak

VMAJOR = 1
VMINOR = 0
VMICRO = 0

obj-y := tsana.o

NAME = tsana
TYPE = exe

CFLAGS += -I../libzutil
CFLAGS += -I../libzbuddy
CFLAGS += -I../libzts
CFLAGS += -I../libzlst
CFLAGS += -I../libparam_xml

ifeq ($(SYS),WINDOWS)
CFLAGS += -I/include/libxml2
LDFLAGS += -L/lib -lxml2
else
CFLAGS += -I/usr/include/libxml2
ifeq ($(ARCH),X86_64)
LDFLAGS += -L/usr/lib/x86_64-linux-gnu -lxml2
else
LDFLAGS += -L/lib -lxml2
endif
endif

LDFLAGS += -L../libzutil -lzutil
LDFLAGS += -L../libzbuddy -lzbuddy
LDFLAGS += -L../libzts -lzts
LDFLAGS += -L../libparam_xml -lparam_xml

include ../common.mak
