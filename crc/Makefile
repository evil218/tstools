EXECUTABLE = test.exe

CC = gcc
CPPFLAGS = -I. -I../include
CFLAGS = -Wall -W -Werror -std=c99
#CFLAGS += -O2
#CFLAGS += -g
CXXFLAGS = $(CFLAGS)
COMPILE = $(CC) $(CPPFLAGS) $(CFLAGS) -c

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c, %.o, $(SRCS))
DEPS := $(patsubst %.c, %.d, $(SRCS))

all: $(EXECUTABLE)

$(EXECUTABLE): $(DEPS) $(OBJS)
	$(CC) -o $(EXECUTABLE) $(OBJS) ../lib/crc.o

%.d: %.c
	@$(CC) -MM $(CPPFLAGS) $< > $@
	@$(CC) -MM $(CPPFLAGS) $< | sed s/.o/.d/ >> $@

%.o: %.c
	$(COMPILE) -o $@ $<

clean:
	-rm -f $(OBJS) $(EXECUTABLE) $(DEPS) *~

explain:
	@echo "Source files: $(SRCS)"
	@echo "Object files: $(OBJS)"
	@echo "Dependency files: $(DEPS)"

depend: $(DEPS)
	@echo "Dependency files are now up-to-date."

-include $(DEPS)

# other things

INSTALL_DIR = C:/windows/system32

$(INSTALL_DIR)/%.exe: %.exe
	cp $< $@

install: $(INSTALL_DIR)/$(EXECUTABLE)

uninstall:
	-rm -f $(INSTALL_DIR)/$(EXECUTABLE)

ctags:
	ctags -R .
