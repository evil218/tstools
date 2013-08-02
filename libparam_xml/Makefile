#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

VMAJOR = 1
VMINOR = 0
VRELEA = 0

obj-y := param_xml.o

NAME = param_xml
TYPE = lib
DESC = parameter xml convertor
HEADERS = param_xml.h

CFLAGS += -I../libzlst
CFLAGS += -I/usr/include/libxml2

LDFLAGS += -L../libzlst -lzlst

ifeq ($(ARCH),X86_64)
LDFLAGS += -L/usr/lib/x86_64-linux-gnu -lxml2
else
LDFLAGS += -L/usr/lib -lxml2
endif

include ../common.mak
