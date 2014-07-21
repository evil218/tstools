#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := buddy.o

VMAJOR = 1
VMINOR = 1
VPATCH = 0
NAME = zbuddy
TYPE = lib
DESC = buddy memory pool, to avoid malloc and free from OS frequently
HEADERS = buddy.h
INCDIRS := -I. -I..
CFLAGS += $(INCDIRS)

LDFLAGS += -lpthread

LINTFLAGS := -formatcode

include ../common.mak
