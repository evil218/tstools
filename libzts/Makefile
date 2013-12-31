#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := ts.o

VMAJOR = 1
VMINOR = 1
VPATCH = 0
NAME = zts
TYPE = lib
DESC = analyse ts stream
HEADERS = ts.h
INCDIRS := -I. -I..
INCDIRS += -I../libzlst
INCDIRS += -I../libzbuddy

CFLAGS += $(INCDIRS)

LDFLAGS += -L../libzlst -lzlst
LDFLAGS += -L../libzbuddy -lzbuddy

include ../common.mak
