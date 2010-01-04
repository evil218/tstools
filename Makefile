DIRS = tscat ipcat tsana tobin

all:
	-@$(MAKE) -C lib $@
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

doc:
	-@$(MAKE) -C lib all
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

install:
	-@$(MAKE) -C lib all
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

uninstall:
	for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

ctags:
	ctags -R .
