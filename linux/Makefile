# Makefile for linux
#
# Take a look at the beginning,
# modify the variables to suit your environment.
# Having done that, you can do a
#
#   make [all]
#   make lib
#   make prj
#   make clean
#   make rebuild
#   make install
#   make uninstall
#
# 20120212, ZHOU Cheng <zhoucheng@tsinghua.org.cn>

# Do not print "Entering directory ..."
MAKEFLAGS += --no-print-directory

# env
PLATFORM = linux

ifeq ($(DEBUG),1)
TYPE = debug
else
TYPE = release
endif

# names and dirs
NAME = ts1
LIB_NAME = lib$(NAME).a

LIB_SRCDIR = ../libts

PRJ_NAME = tstools
PRJ_SRCDIR = ../src

INSTALLDIR = /usr/local/bin

# files
LIB_DEP = lib.d

LIB_OBJS  = libcrc.o
LIB_OBJS += libif.o
LIB_OBJS += libzlst.o
LIB_OBJS += libbuddy.o
LIB_OBJS += libts.o

LIB_SRCS = $(subst .o,.c,$(subst lib,$(LIB_SRCDIR)/,$(LIB_OBJS)))

PRJ_DEP = prj.d

PRJ_OBJS  = prjudp.o
PRJ_OBJS += prjurl.o
PRJ_OBJS += prjUTF_GB.o
PRJ_OBJS += prjcatts.o
PRJ_OBJS += prjcatip.o
PRJ_OBJS += prjtsana.o
PRJ_OBJS += prjtobin.o
PRJ_OBJS += prjdtsdi.o
PRJ_OBJS += prjtopcm.o
PRJ_OBJS += prjatsc_mh_tcp.o

PRJ_SRCS = $(subst .o,.c,$(subst prj,$(PRJ_SRCDIR)/,$(PRJ_OBJS)))

PRJ_AIMS  = catts
PRJ_AIMS += catip
PRJ_AIMS += tsana
PRJ_AIMS += tobin
PRJ_AIMS += dtsdi
PRJ_AIMS += topcm

# includes
INCFLAGS  = -I. -I..
INCFLAGS += -I../include/libts

# compiler and its options
CC = gcc
CFLAGS += -Wall -Werror
CFLAGS += -std=gnu99
CFLAGS += -DPLATFORM_$(PLATFORM)
ifeq ($(DEBUG),1)
CFLAGS += -D_DEBUG -g
else
CFLAGS += -DNDEBUG -O2
endif

# archiver and its options
AR = ar
ARFLAGS = -r

# linker and its options
LD = gcc
LDFLAGS += 
LIBS += -L. -l$(NAME)

# aims
all: lib prj

lib: libdep $(LIB_NAME)

prj: prjdep $(PRJ_AIMS)

clean:
	-rm -f lib.d $(LIB_NAME) $(LIB_OBJS)
	-rm -f prj.d $(PRJ_AIMS) $(PRJ_OBJS)

rebuild: clean all

install:
	-cp catts $(INSTALLDIR)
	-cp catip $(INSTALLDIR)
	-cp tsana $(INSTALLDIR)
	-cp tobin $(INSTALLDIR)
	-cp dtsdi $(INSTALLDIR)
	-cp topcm $(INSTALLDIR)

uninstall:
	-rm -f $(INSTALLDIR)/catts
	-rm -f $(INSTALLDIR)/catip
	-rm -f $(INSTALLDIR)/tsana
	-rm -f $(INSTALLDIR)/tobin
	-rm -f $(INSTALLDIR)/dtsdi
	-rm -f $(INSTALLDIR)/topcm

# creates the dependency file
libdep: $(LIB_DEP)

$(LIB_DEP): $(LIB_SRCS)
	@echo make $@ for $(LIB_SRCS)
	@$(CC) $(INCFLAGS) -MM $(LIB_SRCS) > $@

prjdep: $(PRJ_DEP)

$(PRJ_DEP): $(PRJ_SRCS)
	@echo make $@ for $(PRJ_SRCS)
	@$(CC) $(INCFLAGS) -MM $(PRJ_SRCS) > $@

version:
	../version.sh > ../version.h

# rule for compilation
lib%.o: $(LIB_SRCDIR)/%.c
	-$(CC) $(CFLAGS) $(INCFLAGS) -o $@ -c $<

prj%.o: $(PRJ_SRCDIR)/%.c
	-$(CC) $(CFLAGS) $(INCFLAGS) -o $@ -c $<

# creates the aim
$(LIB_NAME): $(LIB_OBJS) $(LIB_DEP)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

catts: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj$@.o $(LIBS)

catip: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj$@.o $(LIBS) prjudp.o prjurl.o

tsana: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj$@.o $(LIBS) prjUTF_GB.o prjatsc_mh_tcp.o

tobin: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj$@.o $(LIBS)

dtsdi: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj$@.o $(LIBS)

topcm: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj$@.o $(LIBS)

# Source dependencies
-include $(LIB_DEP)
-include $(PRJ_DEP)

# THE END
