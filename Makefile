# Makefile
# 
# 20130730, ZHOU Cheng <zhoucheng@tsinghua.org.cn>
# 20130522, ZHOU Cheng <zhoucheng@tsinghua.org.cn>

# Do not print "Entering directory ..."
MAKEFLAGS += --no-print-directory

LIB_DIRS := libzutil
LIB_DIRS += libzlst
LIB_DIRS += libzbuddy
LIB_DIRS += libzts
LIB_DIRS += libparam_xml

EXE_DIRS := catts
EXE_DIRS += catip
EXE_DIRS += tsana
EXE_DIRS += tobin

define make_lib_dirs
	@for dir in $(LIB_DIRS); do $(MAKE) -C $$dir $@; done
endef

define make_exe_dirs
	@for dir in $(EXE_DIRS); do $(MAKE) -C $$dir $@; done
endef

all clean install uninstall:
	$(make_lib_dirs)
	$(make_exe_dirs)

pc:
	$(make_lib_dirs)

distclean: clean
	rm -f config.mak tstool_config.h config.h config.log ts.pc ts.def TAGS

etags: TAGS

TAGS:
	etags $(LIB_SRCS) $(PRJ_SRCS)

mode:
	find . -type f -exec chmod 644 {} \;
	find . -type d -exec chmod 755 {} \;
	chmod 755 config.guess config.sub configure version.sh
