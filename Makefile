install:
	-@$(MAKE) -C bin2txt $@
	-@$(MAKE) -C tsana $@

uninstall:
	-@$(MAKE) -C bin2txt $@
	-@$(MAKE) -C tsana $@

clean:
	-@$(MAKE) -C bin2txt $@
	-@$(MAKE) -C tsana $@

ctags:
	ctags -R .
