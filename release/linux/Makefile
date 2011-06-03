INSTALL_DIR = /usr/local/bin

$(INSTALL_DIR)/%: %
	cp $< $@

all: install

install: \
	$(INSTALL_DIR)/catbin \
	$(INSTALL_DIR)/catip \
	$(INSTALL_DIR)/tsana \
	$(INSTALL_DIR)/tobin \
	$(INSTALL_DIR)/toip

uninstall:
	rm -f $(INSTALL_DIR)/catbin
	rm -f $(INSTALL_DIR)/catip
	rm -f $(INSTALL_DIR)/tsana
	rm -f $(INSTALL_DIR)/tobin
	rm -f $(INSTALL_DIR)/toip
