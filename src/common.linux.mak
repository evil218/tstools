# =============================================================================
# definition
# =============================================================================
CFLAGS = -Wall -Werror -std=c99

OBJ_DIR = ../../release/linux
INSTALL_DIR = /usr/local/bin

# -------------------------------------------------------------------
# debug or release
# -------------------------------------------------------------------
BUILD_TYPE = release

ifeq ($(BUILD_TYPE), release)
	CFLAGS += -O2
else
	CFLAGS += -g
endif

# -------------------------------------------------------------------
# others
# -------------------------------------------------------------------
CC = gcc
CPPFLAGS = -I. -I../include
COMPILE = $(CC) $(CPPFLAGS) $(CFLAGS) -c

OBJS := $(patsubst %.c, %.o, $(SRCS))
DEPS := $(patsubst %.c, %.d, $(SRCS))

WOBJS := $(patsubst %.c, %.obj, $(SRCS))

# =============================================================================
# wildcard rule
# =============================================================================
%.d: %.c
	@echo make dependency file for $<
	@$(CC) -MM $(CPPFLAGS) $< > $@
	@$(CC) -MM $(CPPFLAGS) $< | sed s/\.o:/.d:/ >> $@

%.o: %.c
	$(COMPILE) -o $@ $<

%.1: $(INSTALL_DIR)/%$(POSTFIX)
	-help2man -o $@ $<

$(INSTALL_DIR)/%$(POSTFIX): %$(POSTFIX)

%.html: %.1
	-man2html $< > $@

$(OBJ_DIR)/doc/%.html: %.html
	cp $< $@

# =============================================================================
# supported aim
# =============================================================================
all: $(NAME)$(POSTFIX)

$(NAME).a: $(OBJS) $(DEPS)
	ar r $@ $(OBJS)

$(NAME).exe: $(OBJS) $(DEPS) ../libts/libts1.a ../libnet/libnet1.a
	$(CC) -o $@ $(OBJS) -L../libts -L../libnet -lts1 -lnet1

$(INSTALL_DIR)/$(NAME)$(POSTFIX): $(NAME)$(POSTFIX)
ifneq ($(POSTFIX),.a)
	cp $< $@
endif

$(OBJ_DIR)/$(NAME)$(POSTFIX): $(NAME)$(POSTFIX)
ifneq ($(POSTFIX),.a)
ifneq ($(TERM),cygwin)
	cp $< $@
endif
endif

clean:
	-rm -f $(DEPS) $(OBJS) $(WOBJS) *~ $(NAME).1 $(NAME).html
	-rm -f $(NAME)     $(NAME).a  
	-rm -f $(NAME).exe $(NAME).lib

explain:
	@echo "    Source     files: $(SRCS)"
	@echo "    Object     files: $(OBJS)"
	@echo "    Dependency files: $(DEPS)"

depend: $(DEPS)
	@echo "Dependency files are now up-to-date."

-include $(DEPS)

doc: $(OBJ_DIR)/doc/$(NAME).html

install: $(INSTALL_DIR)/$(NAME)$(POSTFIX)

release: $(OBJ_DIR)/$(NAME)$(POSTFIX)

uninstall:
ifneq ($(POSTFIX),.a)
	-rm -f $(INSTALL_DIR)/$(NAME)$(POSTFIX)
endif

# =============================================================================
# THE END
# =============================================================================
