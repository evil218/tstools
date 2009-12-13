INSTALL_DIR = /cygdrive/c/windows/system32

all:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tobin $@

doc:
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tobin $@

install: $(INSTALL_DIR)/cygwin1.dll
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tobin $@

uninstall:
	-rm $(INSTALL_DIR)/cygwin1.dll
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tobin $@

clean:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tobin $@

explain:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tobin $@

depend:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tobin $@

ctags:
	ctags -R .

$(INSTALL_DIR)/cygwin1.dll: release/cygwin1.dll
	cp $< $@
