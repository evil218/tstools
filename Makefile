INSTALL_DIR = /cygdrive/c/windows/system32
DIRS = tscat ipcat tsana tobin

all:
	-@$(MAKE) -C lib $@
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

doc:
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

clean:
	-@$(MAKE) -C lib $@
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

explain:
	-@$(MAKE) -C lib $@
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

depend:
	-@$(MAKE) -C lib $@
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

ctags:
	ctags -R .

$(INSTALL_DIR)/cygwin1.dll: release/cygwin1.dll
	cp $< $@

install: $(INSTALL_DIR)/cygwin1.dll
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

uninstall:
	-rm $(INSTALL_DIR)/cygwin1.dll
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done
