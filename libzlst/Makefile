#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

obj-y := zlst.o

VMAJOR = 1
VMINOR = 1
VPATCH = 0
NAME = zlst
TYPE = lib
DESC = common bidirection list
HEADERS = zlst.h
INCDIRS := -I. -I..
CFLAGS += $(INCDIRS)

include ../common.mak
