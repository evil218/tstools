# Makefile for linux and cygwin
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
ifeq ($(TERM),cygwin)
PLATFORM = cygwin
else
PLATFORM = linux
endif

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
LIB_DEP  = lib.d
LIB_OBJS = lib_crc.o\
		   lib_if.o\
		   lib_list.o\
		   lib_UTF_GB.o\
		   lib_ts.o
LIB_SRCS = $(subst .o,.c,$(subst lib_,$(LIB_SRCDIR)/,$(LIB_OBJS)))

PRJ_DEP  = prj.d
PRJ_OBJS = prj_catts.o\
		   prj_tsana.o\
		   prj_udp.o\
		   prj_url.o\
		   prj_catip.o\
		   prj_atsc_mh_tcp.o\
		   prj_tots.o
PRJ_SRCS = $(subst .o,.c,$(subst prj_,$(PRJ_SRCDIR)/,$(PRJ_OBJS)))
PRJ_AIMS = catts\
		   tsana\
		   catip\
		   tots

# includes
INCFLAGS = -I.
INCFLAGS += -I../include/libts

# compiler and its options
CC = gcc
CFLAGS += -Wall -Werror
CFLAGS += -std=c99
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

lib: lib_dep $(LIB_NAME)

prj: prj_dep $(PRJ_AIMS)

clean:
	-rm -f lib.d $(LIB_NAME) $(LIB_OBJS)
	-rm -f prj.d $(PRJ_AIMS) $(PRJ_OBJS)

rebuild: clean all

install: $(INSTALLDIR)/catts\
	$(INSTALLDIR)/catip\
	$(INSTALLDIR)/tsana

uninstall:
	-rm -f $(INSTALLDIR)/catts
	-rm -f $(INSTALLDIR)/catip
	-rm -f $(INSTALLDIR)/tsana

# creates the dependency file
lib_dep: $(LIB_DEP)

$(LIB_DEP): $(LIB_SRCS)
	@echo make $@ for $(LIB_SRCS)
	@$(CC) $(INCFLAGS) -MM $(LIB_SRCS) > $@

prj_dep: $(PRJ_DEP)

$(PRJ_DEP): $(PRJ_SRCS)
	@echo make $@ for $(PRJ_SRCS)
	@$(CC) $(INCFLAGS) -MM $(PRJ_SRCS) > $@

# rule for compilation
lib_%.o: $(LIB_SRCDIR)/%.c
	-$(CC) $(CFLAGS) $(INCFLAGS) -o $@ -c $<

prj_%.o: $(PRJ_SRCDIR)/%.c
	-$(CC) $(CFLAGS) $(INCFLAGS) -o $@ -c $<

# creates the aim
$(LIB_NAME): $(LIB_OBJS) $(LIB_DEP)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

catts: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj_$@.o $(LIBS)

catip: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj_$@.o $(LIBS) prj_udp.o prj_url.o

tsana: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj_$@.o $(LIBS) prj_atsc_mh_tcp.o

tots: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ prj_$@.o $(LIBS)

# install
$(INSTALLDIR)/%: %
	-cp $< $@

# Source dependencies
-include $(LIB_DEP)
-include $(PRJ_DEP)

# THE END
