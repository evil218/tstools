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

$(EXECUTABLE): $(DEPS) $(OBJS) $(LIBS)
	$(CC) -o $(EXECUTABLE) $(OBJS) $(LIBS)

clean:
	-rm -f $(OBJS) $(EXECUTABLE) $(DEPS) *~

explain:
	@echo "    Source     files: $(SRCS)"
	@echo "    Object     files: $(OBJS)"
	@echo "    Dependency files: $(DEPS)"

depend: $(DEPS)
	@echo "Dependency files are now up-to-date."

-include $(DEPS)

# other things

INSTALL_DIR = /cygdrive/c/windows/system32

$(INSTALL_DIR)/%.exe: %.exe
	cp $< $@

install: $(INSTALL_DIR)/$(EXECUTABLE)

uninstall:
	-rm -f $(INSTALL_DIR)/$(EXECUTABLE)
