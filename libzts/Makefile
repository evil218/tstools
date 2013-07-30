#
# Makefile for libzlst
#

include ../config.mak

VMAJOR = 1
VMINOR = 0
VMICRO = 0

obj-y := ts.o

NAME = zts
TYPE = lib
HEADERS = ts.h

CFLAGS += -I../libzlst
CFLAGS += -I../libzbuddy

LDFLAGS += -L../libzlst -lzlst
LDFLAGS += -L../libzbuddy -lzbuddy

include ../common.mak
