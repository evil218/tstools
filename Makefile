DIRS = lib bin2txt tsana update crc

all:
	@for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

install:
	@for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

uninstall:
	@for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

clean:
	@for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

explain:
	@for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

depend:
	@for dir in $(DIRS); do $(MAKE) -C $$dir $@; done

ctags:
	ctags -R .
