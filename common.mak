#
# common makefile
#

SRCS := $(obj-y:%.o=%.c)

ifeq ($(SYS),WINDOWS)
LIB_SHARED = lib$(NAME)-$(VMAJOR).$(VMINOR).$(VMICRO).dll
LIB_STATIC = lib$(NAME)-$(VMAJOR).$(VMINOR).$(VMICRO).lib
endif

ifeq ($(SYS),CYGWIN)
LIB_SHARED = lib$(NAME)-$(VMAJOR).$(VMINOR).$(VMICRO).dll
LIB_STATIC = lib$(NAME)-$(VMAJOR).$(VMINOR).$(VMICRO).a
endif

ifeq ($(SYS),LINUX)
LIB_SHARED = lib$(NAME).so.$(VMAJOR).$(VMINOR).$(VMICRO)
LIB_STATIC = lib$(NAME)-$(VMAJOR).$(VMINOR).$(VMICRO).a
endif

IMPLIBNAME = lib$(NAME).dll.a

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

.PHONY: all head clean distclean install uninstall

$(LIB_STATIC): .depend $(obj-y)
	$(AR)$@ $(obj-y)
	$(if $(RANLIB), $(RANLIB) $@)

$(LIB_SHARED): .depend $(obj-y)
	$(LD)$@ $(obj-y) $(SOFLAGS) $(LDFLAGS)

$(NAME)$(EXE): .depend $(obj-y)
	$(LD)$@ $(obj-y) $(LDFLAGS)

head: $(HEADERS)
	$(foreach HEADER, $(HEADERS), cp $(HEADER) ../include;)

.depend:
	@rm -f .depend
	@$(foreach SRC, $(SRCS), $(CC) $(CFLAGS) $(SRC) $(DEPMM) 1>> .depend;)

depend: .depend
ifneq ($(wildcard .depend),)
include .depend
endif

clean:
ifeq ($(TYPE),lib)
	rm -f .depend $(obj-y) lib$(NAME)*
else
	rm -f .depend $(obj-y) $(NAME)$(EXE)
endif

install: $(aim)
	install -d $(bindir)
	-install -m 755 $(aim) $(bindir)
#	-ldconfig $(bindir)

uninstall:
ifeq ($(TYPE),lib)
	-rm -f $(bindir)/lib$(NAME)*
#	-ldconfig $(bindir)
else
	-rm -f $(bindir)/$(NAME)$(EXE)
endif
