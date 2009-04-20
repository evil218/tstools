FLAG = -std=c99

all: tsflt.exe ts2es.exe tsana.exe

%.exe: %.c
	gcc $(FLAG) -o $@ $<

install:
	cp *.exe C:/windows/system32

uninstall:
	rm -f C:/windows/system32/tsflt.exe
	rm -f C:/windows/system32/ts2es.exe
	rm -f C:/windows/system32/tsana.exe

clean:
	rm -f *.exe
