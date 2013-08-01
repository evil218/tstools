#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

VMAJOR = 1
VMINOR = 0
VRELEA = 0

obj-y := ts.o

NAME = zts
TYPE = lib
DESC = analyse ts stream
HEADERS = ts.h

CFLAGS += -I../libzlst
CFLAGS += -I../libzbuddy

LDFLAGS += -L../libzlst -lzlst
LDFLAGS += -L../libzbuddy -lzbuddy

include ../common.mak
