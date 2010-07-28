DIRS = bincat ipcat tsana tobin

# Do not print "Entering directory ..."
MAKEFLAGS += --no-print-directory

all:
	@echo
	@echo "---> lib:"
	-@$(MAKE) -C lib $@
	@for dir in $(DIRS); do echo; echo "---> $$dir:"; $(MAKE) -C $$dir $@; done

doc:
	@echo
	@echo "---> lib:"
	-@$(MAKE) -C lib all
	@for dir in $(DIRS); do echo; echo "---> $$dir:"; $(MAKE) -C $$dir $@; done

clean:
	@echo
	@echo "---> lib:"
	-@$(MAKE) -C lib $@
	@for dir in $(DIRS); do echo; echo "---> $$dir:"; $(MAKE) -C $$dir $@; done

explain:
	@echo
	@echo "---> lib:"
	-@$(MAKE) -C lib $@
	@for dir in $(DIRS); do echo; echo "---> $$dir:"; $(MAKE) -C $$dir $@; done

depend:
	@echo
	@echo "---> lib:"
	-@$(MAKE) -C lib $@
	@for dir in $(DIRS); do echo; echo "---> $$dir:"; $(MAKE) -C $$dir $@; done

install:
	@echo
	@echo "---> lib:"
	-@$(MAKE) -C lib all
	@for dir in $(DIRS); do echo; echo "---> $$dir:"; $(MAKE) -C $$dir $@; done

uninstall:
	@for dir in $(DIRS); do echo; echo "---> $$dir:"; $(MAKE) -C $$dir $@; done

ctags:
	ctags -R .
