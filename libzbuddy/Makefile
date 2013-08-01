#
# Makefile for libzlst
#

ifneq ($(wildcard ../config.mak),)
include ../config.mak
endif

VMAJOR = 1
VMINOR = 0
VRELEA = 0

obj-y := buddy.o

NAME = zbuddy
TYPE = lib
DESC = buddy memory pool, to avoid malloc and free from OS frequently
HEADERS = buddy.h

include ../common.mak
