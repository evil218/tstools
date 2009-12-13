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

%.1: %.exe
	-help2man -o $@ $<

%.html: %.1
	-man2html $< > $@

$(NAME).exe: $(DEPS) $(OBJS) $(LIBS)
	$(CC) -o $(NAME).exe $(OBJS) $(LIBS)

doc: $(NAME).html

clean:
	-rm -f $(OBJS) $(NAME).exe $(NAME).1 $(NAME).html $(DEPS) *~

explain:
	@echo "    Source     files: $(SRCS)"
	@echo "    Object     files: $(OBJS)"
	@echo "    Dependency files: $(DEPS)"

depend: $(DEPS)
	@echo "Dependency files are now up-to-date."

-include $(DEPS)

# other things

INSTALL_DIR = /cygdrive/c/windows/system32

../release/%.exe: %.exe
	cp $< $@
	cp $< $(INSTALL_DIR)

../release/%.html: %.html
	cp $< $@

install: \
	../release/$(NAME).exe \
	../release/$(NAME).html

uninstall:
	-rm -f $(INSTALL_DIR)/$(NAME).exe
	-rm -f ../release/$(NAME).exe
	-rm -f ../release/$(NAME).html
