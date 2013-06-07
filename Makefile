MAKEFLAGS += --no-print-directory

LIBS = libzutil libzbuddy libzlst libparam_xml libzts

define make_dirs
	@for dir in $(LIBS); do echo; echo "---> $$dir:"; $(MAKE) -C $$dir $@; done
endef

all install uninstall clean:
	$(make_dirs)

distclean:
	rm -f config.mak tstool_config.h config.h config.log ts.pc ts.def TAGS

mode:
	find . -type f -exec chmod 644 {} \;
	find . -type d -exec chmod 755 {} \;
	chmod 755 config.guess config.sub configure version.sh
