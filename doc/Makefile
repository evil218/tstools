tstools.html: tstools.txt
	asciidoc -n -a toc $<

../release/doc/tstools.html: tstools.html
	mv $< $@

all: tstools.html

release: ../release/doc/tstools.html

clean:
	rm tstools.html
