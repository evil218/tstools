#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := param_xml.o

VMAJOR = 1
VMINOR = 1
VPATCH = 0
NAME = param_xml
TYPE = lib
DESC = parameter xml convertor
HEADERS = param_xml.h
INCDIRS := -I. -I..
INCDIRS += -I../libzlst
INCDIRS += -I/usr/include/libxml2
CFLAGS += $(INCDIRS)
CFLAGS += -D_PARAM_XML_EXPORT

LDFLAGS += -L../libzlst -lzlst

ifeq ($(ARCH),X86_64)
LDFLAGS += -L/usr/lib/x86_64-linux-gnu -lxml2
else
LDFLAGS += -L/usr/lib -lxml2
endif

include ../common.mak
