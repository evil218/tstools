# Makefile
# 
# 20130730, ZHOU Cheng <zhoucheng@tsinghua.org.cn>
# 20130522, ZHOU Cheng <zhoucheng@tsinghua.org.cn>

# Do not print "Entering directory ..."
MAKEFLAGS += --no-print-directory

DIRS := libzutil
DIRS += libzlst
DIRS += libzbuddy
DIRS += libzts
DIRS += libparam_xml
DIRS += catts
DIRS += catip
DIRS += tsana
DIRS += tobin

define make_dirs
	@for dir in $(DIRS); do $(MAKE) -C $$dir $@; done
endef

all head clean install uninstall:
	$(make_dirs)

distclean: clean
	rm -f config.mak tstool_config.h config.h config.log ts.pc ts.def TAGS

etags: TAGS

TAGS:
	etags $(LIB_SRCS) $(PRJ_SRCS)

mode:
	find . -type f -exec chmod 644 {} \;
	find . -type d -exec chmod 755 {} \;
	chmod 755 config.guess config.sub configure version.sh
