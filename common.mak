LIB_STATIC = lib$(NAME).a

IMPLIBNAME=lib$(NAME).dll.a
LIB_XNAME = lib$(NAME).so
LIB_SONAME = $(LIB_XNAME).$(VMAJOR)
LIB_NAME = $(LIB_SONAME).$(VMINOR).$(VMICRO)

OBJS = $(SRCS:%.c=%.o)

include ../config.mak

.PHONY: all default clean lib-static lib-shared install-lib-dev install-lib-static install-lib-shared

all: default

lib-static: $(LIB_STATIC)
lib-shared: $(LIB_NAME)

$(LIB_STATIC): .depend $(OBJS)
	@rm -f $@
	$(AR)$@ $(OBJS)
	$(if $(RANLIB), $(RANLIB) $@)

$(LIB_NAME): .depend $(OBJS) $(OBJSO)
	$(LD)$@ $(OBJS) $(OBJSO) $(SOFLAGS) $(LDFLAGS)

.depend: ../config.mak
	@rm -f .depend
	@echo $@
	@$(foreach SRC, $(addprefix $(SRCPATH)/, $(SRCS)), $(CC) $(CFLAGS) $(SRC) $(DEPMT) $(SRC:$(SRCPATH)/%.c=%.o) $(DEPMM) 1>> .depend;)

depend: .depend
ifneq ($(wildcard .depend),)
include .depend
endif

clean:
	rm -f $(LIB_STATIC) $(LIB_NAME) $(IMPLIBNAME) $(OBJS) .depend

install-lib-dev:
	install -d $(DESTDIR)$(includedir)
	install -m 644 $(HEADERS) $(DESTDIR)$(includedir)

install-lib-static: lib-static install-lib-dev
	install -d $(DESTDIR)$(libdir)
	install -m 644 $(LIB_STATIC) $(DESTDIR)$(libdir)

install-lib-shared: lib-shared install-lib-dev
	install -d $(DESTDIR)$(libdir)
	install -m 644 $(IMPLIBNAME) $(DESTDIR)$(libdir)
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(LIB_NAME) $(DESTDIR)$(bindir)
	ln -f -s $(DESTDIR)$(bindir)/$(LIB_NAME) $(DESTDIR)$(bindir)/$(LIB_SONAME)
	ln -f -s $(DESTDIR)$(bindir)/$(LIB_SONAME) $(DESTDIR)$(bindir)/$(LIB_XNAME)
#	ldconfig $(DESTDIR)$(bindir)

uninstall:
	rm -f $(addprefix $(DESTDIR)$(includedir)/, $(HEADERS))
	rm -f $(DESTDIR)$(libdir)/lib$(NAME)*
	rm -f $(DESTDIR)$(bindir)/lib$(NAME)*
