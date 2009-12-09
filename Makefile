INSTALL_DIR = /cygdrive/c/windows/system32

all:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@

install: $(INSTALL_DIR)/cygwin1.dll
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@

uninstall:
	-rm $(INSTALL_DIR)/cygwin1.dll
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@

clean:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@

explain:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@

depend:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@

ctags:
	ctags -R .

$(INSTALL_DIR)/cygwin1.dll: release/cygwin1.dll
	cp $< $@
