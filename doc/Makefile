all: catts.html catip.html tsana.html tobin.html tstools

catts.1:
	-help2man -o $@ catts

catip.1:
	-help2man -o $@ catip

tsana.1:
	-help2man -o $@ tsana

tobin.1:
	-help2man -o $@ tobin

%.html: %.1
	-man2html $< > $@
	rm $<

tstools: tstools.txt
	asciidoc -n -a toc $<
