INSTALL_DIR = /usr/local/bin

$(INSTALL_DIR)/%: %
	cp $< $@

all: install

install: \
	$(INSTALL_DIR)/catts \
	$(INSTALL_DIR)/catip \
	$(INSTALL_DIR)/tsana \
	$(INSTALL_DIR)/tots \
	$(INSTALL_DIR)/toip

uninstall:
	rm -f $(INSTALL_DIR)/catts
	rm -f $(INSTALL_DIR)/catip
	rm -f $(INSTALL_DIR)/tsana
	rm -f $(INSTALL_DIR)/tots
	rm -f $(INSTALL_DIR)/toip
