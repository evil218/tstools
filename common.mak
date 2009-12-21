# windows or linux
ifeq ($(TERM),cygwin)
EXE_POSTFIX = .exe
else
EXE_POSTFIX =
endif

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

%.d: %.c
	@echo make dependency file for $<
	@$(CC) -MM $(CPPFLAGS) $< > $@
	@$(CC) -MM $(CPPFLAGS) $< | sed s/.o/.d/ >> $@

%.o: %.c
	$(COMPILE) -o $@ $<

%.1: %$(EXE_POSTFIX)
	-help2man -o $@ $<

%.html: %.1
	-man2html $< > $@

$(NAME).a: $(OBJS)
	ar r $@ $(OBJS)

$(NAME)$(EXE_POSTFIX): $(DEPS) $(OBJS) ../lib/libts.a
	$(CC) -o $(NAME)$(EXE_POSTFIX) $(OBJS) -L../lib -lts

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

# other things

INSTALL_DIR = /cygdrive/c/windows/system32

../release/%$(EXE_POSTFIX): %$(EXE_POSTFIX)
	cp $< $@
	cp $< $(INSTALL_DIR)

../release/%.html: %.html
	cp $< $@

doc: ../release/$(NAME).html

install: ../release/$(NAME)$(EXE_POSTFIX)

uninstall:
	-rm -f $(INSTALL_DIR)/$(NAME)$(EXE_POSTFIX)
	-rm -f ../release/$(NAME)$(EXE_POSTFIX)
	-rm -f ../release/$(NAME).html
