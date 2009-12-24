# =============================================================================
# definition
# =============================================================================

# -------------------------------------------------------------------
# windows or linux
# -------------------------------------------------------------------
ifeq ($(TERM),cygwin)

# windows
ifeq ($(NAME),libts)
POSTFIX = .a
else
POSTFIX = .exe
endif
INSTALL_DIR = /cygdrive/c/windows/system32

else # neq ($(TERM),cygwin)

# linux
ifeq ($(NAME),libts)
POSTFIX = .a
else
POSTFIX =
endif
INSTALL_DIR = /cygdrive/c/windows/system32

endif # ($(TERM),cygwin)

# -------------------------------------------------------------------
# others
# -------------------------------------------------------------------
CC = gcc
CPPFLAGS = -I. -I../lib
CFLAGS = -Wall -W -Werror
CFLAGS += -std=c99
CFLAGS += -O2
#CFLAGS += -g
CXXFLAGS = $(CFLAGS)
COMPILE = $(CC) $(CPPFLAGS) $(CFLAGS) -c

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c, %.o, $(SRCS))
DEPS := $(patsubst %.c, %.d, $(SRCS))

# =============================================================================
# wildcard rule
# =============================================================================
%.d: %.c
	@echo make dependency file for $<
	@$(CC) -MM $(CPPFLAGS) $< > $@
	@$(CC) -MM $(CPPFLAGS) $< | sed s/.o/.d/ >> $@

%.o: %.c
	$(COMPILE) -o $@ $<

%.1: %$(POSTFIX)
	-help2man -o $@ $<

%.html: %.1
	-man2html $< > $@

# =============================================================================
# supported aim
# =============================================================================
all: $(NAME)$(POSTFIX)

$(NAME).a: $(OBJS)
	ar r $@ $(OBJS)

$(NAME).exe: $(OBJS) $(DEPS) ../lib/libts.a
	$(CC) -o $@ $(OBJS) -L../lib -lts

$(NAME): $(OBJS) $(DEPS) ../lib/libts.a
	$(CC) -o $@ $(OBJS) -L../lib -lts

clean:
	-rm -f $(OBJS) $(DEPS) *~
	-rm -f $(NAME) $(NAME).exe $(NAME).a $(NAME).1 $(NAME).html

explain:
	@echo "    Source     files: $(SRCS)"
	@echo "    Object     files: $(OBJS)"
	@echo "    Dependency files: $(DEPS)"

depend: $(DEPS)
	@echo "Dependency files are now up-to-date."

-include $(DEPS)

../release/%$(POSTFIX): %$(POSTFIX)
	cp $< $@
	cp $< $(INSTALL_DIR)

../release/%.html: %.html
	cp $< $@

doc: ../release/$(NAME).html

install: ../release/$(NAME)$(POSTFIX)

uninstall:
	-rm -f $(INSTALL_DIR)/$(NAME)$(POSTFIX)
	-rm -f ../release/$(NAME)$(POSTFIX)
	-rm -f ../release/$(NAME).html

# =============================================================================
# THE END
# =============================================================================
