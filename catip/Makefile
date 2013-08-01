#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

VMAJOR = 1
VMINOR = 0
VRELEA = 0

obj-y := catip.o

NAME = catip
TYPE = exe

CFLAGS += -I../libzutil

LDFLAGS += -L../libzutil -lzutil

include ../common.mak
