FLAG = -Wall -W -Werror
#FLAG += -O2
#FLAG += -g
FLAG += -std=c99 # too slow, why?

INSTALL_DIR = C:/windows/system32

all: exe

exe: bin2txt.exe tsflt.exe ts2es.exe tsana.exe

%.o: %.c Makefile
	gcc $(FLAG) -c $<

%.exe: %.o Makefile
	gcc $(FLAG) -o $@ $<

$(INSTALL_DIR)/%.exe: %.exe
	cp $< $@

install: $(INSTALL_DIR)/bin2txt.exe \
	$(INSTALL_DIR)/tsflt.exe \
	$(INSTALL_DIR)/ts2es.exe \
	$(INSTALL_DIR)/tsana.exe

uninstall:
	rm -f $(INSTALL_DIR)/bin2txt.exe
	rm -f $(INSTALL_DIR)/tsflt.exe
	rm -f $(INSTALL_DIR)/ts2es.exe
	rm -f $(INSTALL_DIR)/tsana.exe

clean:
	rm -f *.o *.exe
