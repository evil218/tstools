FLAG = -Wall -W -Werror
#FLAG += -O2
#FLAG += -g
#FLAG += -std=c99

INSTALL_DIR = C:/windows/system32

all: exe

exe: bin2txt.exe tsana.exe

%.o: %.c Makefile
	gcc $(FLAG) -std=c99 -c $<

%.exe: %.o Makefile
	gcc $(FLAG) -std=c99 -o $@ $<

bin2txt.o: bin2txt.c Makefile
	gcc $(FLAG) -c $<

bin2txt.exe: bin2txt.o Makefile
	gcc $(FLAG) -o $@ $<

$(INSTALL_DIR)/%.exe: %.exe
	cp $< $@

install: $(INSTALL_DIR)/bin2txt.exe \
	$(INSTALL_DIR)/tsana.exe

uninstall:
	rm -f $(INSTALL_DIR)/bin2txt.exe
	rm -f $(INSTALL_DIR)/tsana.exe

clean:
	rm -f *.o *.exe
