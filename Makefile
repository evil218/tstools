FLAG = -Wall -W -Werror
#FLAG += -O2
#FLAG += -g

INSTALL_DIR = C:/windows/system32

all: exe

exe: bin2txt.exe tsana.exe

bin2txt.exe:
	gcc $(FLAG) -o $@ bin2txt.c

list.o:
	gcc $(FLAG) -std=c99 -c list.c

tsana.o:
	gcc $(FLAG) -std=c99 -c tsana.c

tsana.exe:
	gcc $(FLAG) -o $@ list.o tsana.o

$(INSTALL_DIR)/%.exe: %.exe
	cp $< $@

install: $(INSTALL_DIR)/bin2txt.exe \
	$(INSTALL_DIR)/tsana.exe

uninstall:
	rm -f $(INSTALL_DIR)/bin2txt.exe
	rm -f $(INSTALL_DIR)/tsana.exe

ctags:
	ctags -R .

clean:
	rm -f *.o *.exe


bin2txt.exe: bin2txt.c \
	Makefile

list.o: list.c \
	list.h \
	Makefile

tsana.o: tsana.c \
	list.h \
	Makefile

tsana.exe: Makefile \
	list.o \
	tsana.o

