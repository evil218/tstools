all: tsflt.exe ts2es.exe tsana.exe

tsflt.exe: tsflt.c
	gcc -o tsflt $<

ts2es.exe: ts2es.c
	gcc -o ts2es $<

tsana.exe: tsana.c
	gcc -o tsana $<

install:
	cp *.exe /c/windows/system32

uninstall:
	rm -f /c/windows/system32/tsflt.exe
	rm -f /c/windows/system32/ts2es.exe
	rm -f /c/windows/system32/tsana.exe

clean:
	rm -f *.exe
