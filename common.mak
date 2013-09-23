#
# common makefile
#

SRCS := $(obj-y:%.o=%.c)

ifeq ($(SYS),LINUX)
LIB_SHARED = lib$(NAME).so.$(VMAJOR).$(VMINOR).$(VRELEA)
SONAME = lib$(NAME).so.$(VMAJOR)
LIB_STATIC = lib$(NAME)-$(VMAJOR).$(VMINOR).$(VRELEA).a
else # CYGWIN or WINDOWS
LIB_SHARED = lib$(NAME)-$(VMAJOR).$(VMINOR).$(VRELEA).dll
IMPLIBNAME = lib$(NAME).dll.a
LIB_STATIC = lib$(NAME).a
endif

ifeq ($(TYPE),lib)
ifeq ($(LIB),shared)
aim = $(LIB_SHARED)
else
aim = $(LIB_STATIC)
endif
else
aim = $(NAME)$(EXE)
endif

all: $(aim)

.PHONY: all clean install uninstall install-lib-dev

$(LIB_STATIC): .depend $(obj-y)
	$(AR)$@ $(obj-y)
	$(if $(RANLIB), $(RANLIB) $@)

$(LIB_SHARED): .depend $(obj-y)
	$(LD)$@ $(obj-y) $(SOFLAGS) $(LDFLAGS)
ifneq ($(SONAME),)
	ln -fs $(LIB_SHARED) lib$(NAME).so
endif

$(NAME)$(EXE): .depend $(obj-y)
	$(LD)$@ $(obj-y) $(LDFLAGS)

.depend:
	@rm -f .depend
	@$(foreach SRC, $(SRCS), $(CC) $(CFLAGS) $(SRC) $(DEPMM) 1>> .depend;)

depend: .depend
ifneq ($(wildcard .depend),)
include .depend
endif

pc: lib$(NAME).pc

lib$(NAME).pc: ../config.mak
	@echo make lib$(NAME).pc
	@echo prefix=$(prefix) > lib$(NAME).pc
	@echo exec_prefix=$(exec_prefix) >> lib$(NAME).pc
	@echo libdir=$(libdir) >> lib$(NAME).pc
	@echo includedir=$(includedir) >> lib$(NAME).pc
	@echo >> lib$(NAME).pc
	@echo Name: $(NAME) >> lib$(NAME).pc
	@echo Description: $(DESC) >> lib$(NAME).pc
	@echo Version:  >> lib$(NAME).pc
	@echo Libs: -L$(libdir) -l$(NAME) >> lib$(NAME).pc
	@echo Libs.private: >> lib$(NAME).pc
	@echo Cflags: -I$(includedir) >> lib$(NAME).pc

lint: $(SRCS)
	@echo -----------------------------------------------------------
	-splint $(LINTFLAGS) $(INCDIRS) $(SRCS)

lintw: $(SRCS)
	@echo -----------------------------------------------------------
	-splint -weak $(LINTFLAGS) $(INCDIRS) $(SRCS)

lintc: $(SRCS)
	@echo -----------------------------------------------------------
	-splint -checks $(LINTFLAGS) $(INCDIRS) $(SRCS)

lints: $(SRCS)
	@echo -----------------------------------------------------------
	-splint -strict $(LINTFLAGS) $(INCDIRS) $(SRCS)

install-lib-dev:
	-install -d $(includedir)
	-install -d $(libdir)
	-install -d $(libdir)/pkgconfig
#	-install -m 644 $(NAME).h $(includedir)
	-install -m 644 lib$(NAME).pc $(libdir)/pkgconfig

ifeq ($(TYPE),lib)
clean:
	-rm -f lib$(NAME)* $(obj-y) .depend

install: $(aim) lib$(NAME).pc install-lib-dev
ifneq ($(IMPLIBNAME),)
	-install -m 755 $(aim) $(bindir)
	-install -m 644 $(IMPLIBNAME) $(libdir)
else ifneq ($(SONAME),)
	-install -m 755 $(aim) $(libdir)
	-ln -fs $(aim) $(libdir)/$(SONAME)
	-ldconfig $(libdir)
endif

uninstall:
#	-rm -f $(includedir)/$(NAME).h
	-rm -f $(libdir)/pkgconfig/lib$(NAME).pc
ifneq ($(IMPLIBNAME),)
	-rm -f $(libdir)/lib$(NAME)* $(bindir)/lib$(NAME)*
else ifneq ($(SONAME),)
	-rm -f $(libdir)/lib$(NAME)*
	-ldconfig $(libdir)
endif


else # exe
clean:
	-rm -f $(NAME)$(EXE) $(obj-y) .depend

install: $(aim)
	-install -m 755 $(aim) $(bindir)

uninstall:
	-rm -f $(bindir)/$(aim)
endif
