FLAG = -g
# FLAG += -Wall -W
FLAG += -std=c99

INSTALL_DIR = C:/windows/system32

all: exe

exe: bin2txt.exe tsflt.exe ts2es.exe tsana.exe

install: $(INSTALL_DIR)/bin2txt.exe \
	$(INSTALL_DIR)/tsflt.exe \
	$(INSTALL_DIR)/ts2es.exe \
	$(INSTALL_DIR)/tsana.exe

uninstall:
	rm -f $(INSTALL_DIR)/bin2txt.exe
	rm -f $(INSTALL_DIR)/tsflt.exe
	rm -f $(INSTALL_DIR)/ts2es.exe
	rm -f $(INSTALL_DIR)/tsana.exe

%.o: %.c
	gcc $(FLAG) -c $<

%.exe: %.o
	gcc $(FLAG) -o $@ $<

$(INSTALL_DIR)/%.exe: %.exe
	cp $< $@

clean:
	rm -f *.o *.exe
