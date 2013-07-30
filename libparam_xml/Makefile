#
# Makefile for libzlst
#

include ../config.mak

VMAJOR = 1
VMINOR = 0
VMICRO = 0

obj-y := param_xml.o

NAME = param_xml
TYPE = lib
HEADERS = param_xml.h

CFLAGS += -I../libzlst

LDFLAGS += -L../libzlst -lzlst

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

include ../common.mak
