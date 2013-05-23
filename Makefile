# Makefile for linux, cygwin and mingw
# (need ./configure first)
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
# 20130522, ZHOU Cheng <zhoucheng@tsinghua.org.cn>

include config.mak

ifeq ($(SYS),WINDOWS)
CFLAGS += -I/include/libxml2
LDFLAGS_SOCK = -lwsock32
LDFLAGS_XML = -L/lib -lxml2
else
CFLAGS += -I/usr/include/libxml2
ifeq ($(ARCH),X86_64)
LDFLAGS_XML = -L/usr/lib/x86_64-linux-gnu -lxml2
else
LDFLAGS_XML = -L/lib -lxml2
endif
endif

# Do not print "Entering directory ..."
MAKEFLAGS += --no-print-directory

# files
LIB_SRCS  = libts/crc.c
LIB_SRCS += libts/zlst.c
LIB_SRCS += libts/buddy.c
LIB_SRCS += libts/ts.c

LIB_OBJS += $(LIB_SRCS:%.c=%.o)

PRJ_SRCS  = src/udp.c
PRJ_SRCS += src/url.c
PRJ_SRCS += src/if.c
PRJ_SRCS += src/UTF_GB.c
PRJ_SRCS += src/param_xml.c
PRJ_SRCS += src/catts.c
PRJ_SRCS += src/catip.c
PRJ_SRCS += src/tsana.c
PRJ_SRCS += src/tobin.c
PRJ_SRCS += src/toip.c

PRJ_OBJS += $(PRJ_SRCS:%.c=%.o)

PRJ_AIMS  = catts$(EXE)
PRJ_AIMS += catip$(EXE)
PRJ_AIMS += tsana$(EXE)
PRJ_AIMS += tobin$(EXE)
PRJ_AIMS += toip$(EXE)

# aims
all: lib prj

lib: $(LIBTS)

prj: $(PRJ_AIMS)

clean:
	rm -f $(LIBTS) $(LIB_OBJS) .depend
	rm -f $(PRJ_AIMS) $(PRJ_OBJS) psi.xml psi.xml.gz

distclean: clean
	rm -f config.mak tstool_config.h config.h config.log ts.pc ts.def

rebuild: clean all

install: $(PRJ_AIMS)
	install -d $(DESTDIR)$(bindir)
	install catts$(EXE) $(DESTDIR)$(bindir)
	install catip$(EXE) $(DESTDIR)$(bindir)
	install tsana$(EXE) $(DESTDIR)$(bindir)
	install tobin$(EXE) $(DESTDIR)$(bindir)
	install toip$(EXE) $(DESTDIR)$(bindir)

uninstall:
	rm -f $(DESTDIR)$(bindir)/catts$(EXE)
	rm -f $(DESTDIR)$(bindir)/catip$(EXE)
	rm -f $(DESTDIR)$(bindir)/tsana$(EXE)
	rm -f $(DESTDIR)$(bindir)/tobin$(EXE)
	rm -f $(DESTDIR)$(bindir)/toip$(EXE)

lib-static: $(LIBX264)
lib-shared: $(SONAME)

$(LIBTS): .depend $(LIB_OBJS)
	rm -f $@
	$(AR)$@ $(LIB_OBJS)
	$(if $(RANLIB), $(RANLIB) $@)

$(SONAME): .depend $(LIB_OBJS) $(OBJSO)
	$(LD)$@ $(OBJS) $(OBJSO) $(SOFLAGS) $(LDFLAGS)

catts$(EXE): $(PRJ_OBJS) .depend $(LIBTS)
	$(LD)$@ $(LDFLAGS) src/catts.o $(LIBTS) src/if.o

catip$(EXE): $(PRJ_OBJS) .depend $(LIBTS)
	$(LD)$@ $(LDFLAGS) src/catip.o $(LIBTS) src/if.o src/udp.o src/url.o $(LDFLAGS_SOCK)

tsana$(EXE): $(PRJ_OBJS) .depend $(LIBTS)
	$(LD)$@ $(LDFLAGS) src/tsana.o $(LIBTS) src/if.o src/UTF_GB.o src/param_xml.o $(LDFLAGS_XML)

tobin$(EXE): $(PRJ_OBJS) .depend $(LIBTS)
	$(LD)$@ $(LDFLAGS) src/tobin.o $(LIBTS) src/if.o

toip$(EXE): $(PRJ_OBJS) .depend $(LIBTS)
	$(LD)$@ $(LDFLAGS) src/toip.o $(LIBTS) src/if.o src/udp.o src/url.o $(LDFLAGS_SOCK)

dtsdi$(EXE): $(PRJ_OBJS) .depend $(LIBTS)
	$(LD)$@ $(LDFLAGS) src/dtsdi.o $(LIBTS)

topcm$(EXE): $(PRJ_OBJS) .depend $(LIBTS)
	$(LD)$@ $(LDFLAGS) src/topcm.o $(LIBTS) src/if.o

.depend: config.mak
	@rm -f .depend
	@echo make $@
	@$(foreach SRC, $(addprefix $(SRCPATH)/, $(LIB_SRCS) $(PRJ_SRCS)), $(CC) $(CFLAGS) $(SRC) $(DEPMT) $(SRC:$(SRCPATH)/%.c=%.o) $(DEPMM) 1>> .depend;)

config.mak:
	./configure

depend: .depend
ifneq ($(wildcard .depend),)
include .depend
endif
