all:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C crc $@

install:
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C crc $@

uninstall:
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C crc $@

clean:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C crc $@

explain:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C crc $@

depend:
	-@$(MAKE) -C lib $@
	-@$(MAKE) -C b2t $@
	-@$(MAKE) -C tsana $@
	-@$(MAKE) -C tscat $@
	-@$(MAKE) -C crc $@

ctags:
	ctags -R .
