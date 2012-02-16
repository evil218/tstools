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
LIB_DEP  = $(TYPE)_$(PLATFORM)_lib.d
LIB_OBJS = $(TYPE)_$(PLATFORM)_lib_crc.o\
		   $(TYPE)_$(PLATFORM)_lib_error.o\
		   $(TYPE)_$(PLATFORM)_lib_if.o\
		   $(TYPE)_$(PLATFORM)_lib_list.o\
		   $(TYPE)_$(PLATFORM)_lib_UTF_GB.o\
		   $(TYPE)_$(PLATFORM)_lib_ts.o
LIB_SRCS = $(subst .o,.c,$(subst $(TYPE)_$(PLATFORM)_lib_,$(LIB_SRCDIR)/,$(LIB_OBJS)))

PRJ_DEP  = $(TYPE)_$(PLATFORM)_prj.d
PRJ_OBJS = $(TYPE)_$(PLATFORM)_prj_udp.o\
		   $(TYPE)_$(PLATFORM)_prj_url.o\
		   $(TYPE)_$(PLATFORM)_prj_catts.o\
		   $(TYPE)_$(PLATFORM)_prj_catip.o\
		   $(TYPE)_$(PLATFORM)_prj_tots.o\
		   $(TYPE)_$(PLATFORM)_prj_atsc_mh_tcp.o\
		   $(TYPE)_$(PLATFORM)_prj_tsana.o
PRJ_SRCS = $(subst .o,.c,$(subst $(TYPE)_$(PLATFORM)_prj_,$(PRJ_SRCDIR)/,$(PRJ_OBJS)))
PRJ_AIMS = catts\
		   catip\
		   tots\
		   tsana

# includes
INCFLAGS = -I.
INCFLAGS += -I../include/libts
INCFLAGS += -I../include/net

# compiler and its options
CC = gcc
CFLAGS += -Wall -Werror
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
	-rm -f $(LIB_NAME) $(PRJ_AIMS) $(TYPE)_$(PLATFORM)_*

rebuild: clean all

install: $(INSTALLDIR)/catts\
	$(INSTALLDIR)/catip\
	$(INSTALLDIR)/tots\
	$(INSTALLDIR)/tsana

uninstall:
	-rm -f $(INSTALLDIR)/catts
	-rm -f $(INSTALLDIR)/catip
	-rm -f $(INSTALLDIR)/tots
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
$(TYPE)_$(PLATFORM)_lib_%.o: $(LIB_SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCFLAGS) -o $@ -c $<

$(TYPE)_$(PLATFORM)_prj_%.o: $(PRJ_SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCFLAGS) -o $@ -c $<

# creates the aim
$(LIB_NAME): $(LIB_OBJS) $(LIB_DEP)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

catts: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ $(TYPE)_$(PLATFORM)_prj_$@.o $(LIBS)

catip: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ $(TYPE)_$(PLATFORM)_prj_$@.o $(LIBS)\
		$(TYPE)_$(PLATFORM)_prj_udp.o\
		$(TYPE)_$(PLATFORM)_prj_url.o

tots: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ $(TYPE)_$(PLATFORM)_prj_$@.o $(LIBS)

tsana: $(PRJ_OBJS) $(PRJ_DEP) $(LIB_NAME)
	$(LD) $(LDFLAGS) -o $@ $(TYPE)_$(PLATFORM)_prj_$@.o $(LIBS)\
		$(TYPE)_$(PLATFORM)_prj_atsc_mh_tcp.o

# install
$(INSTALLDIR)/%: %
	-cp $< $@

# Source dependencies
#-include $(LIB_DEP)
#-include $(PRJ_DEP)

# THE END
