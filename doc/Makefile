all: catts.html catip.html tsana.html tobin.html toip.html tstools buddy

catts.1:
	-help2man -o $@ catts

catip.1:
	-help2man -o $@ catip

tsana.1:
	-help2man -o $@ tsana

tobin.1:
	-help2man -o $@ tobin

toip.1:
	-help2man -o $@ toip

%.html: %.1
	-man2html $< > $@
	rm $<

tstools: tstools.txt
	asciidoc -n -a toc $<

buddy: buddy.txt
	asciidoc -n -a toc $<
