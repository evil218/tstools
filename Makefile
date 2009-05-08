FLAG = -Wall -W -Werror
#FLAG += -O2
#FLAG += -g

INSTALL_DIR = C:/windows/system32

all: exe

exe: bin2txt.exe tsana.exe

bin2txt.exe: bin2txt.c Makefile
	gcc $(FLAG) -o $@ $<

list.o: list.c list.h Makefile
	gcc $(FLAG) -std=c99 -c $<

tsana.o: tsana.c list.h Makefile
	gcc $(FLAG) -std=c99 -c $<

tsana.exe: Makefile list.o tsana.o
	gcc $(FLAG) -o $@ list.o tsana.o

$(INSTALL_DIR)/%.exe: %.exe
	cp $< $@

install: $(INSTALL_DIR)/bin2txt.exe \
	$(INSTALL_DIR)/tsana.exe

uninstall:
	rm -f $(INSTALL_DIR)/bin2txt.exe
	rm -f $(INSTALL_DIR)/tsana.exe

clean:
	rm -f *.o *.exe
