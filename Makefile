CC = gcc
CFLAGS = -Wall -O3 -fPIC -Idsp -Iutils
LDFLAGS = -shared

# Version
VERSION = $(shell cat VERSION 2>/dev/null || echo "1.0.0")

# Plugin names
PLUGINS = speakereq2x2
PRODUCTION = speakereq2x2

# Installation directories
PREFIX ?= /usr/local
DESTDIR ?=
LADSPA_INSTALL_DIR = $(DESTDIR)$(PREFIX)/lib/ladspa

.PHONY: all ladspa clean install install-ladspa uninstall diagram deb deb-clean

all: ladspa diagram

ladspa: speakereq2x2.so

diagram: architecture.png

architecture.png: architecture.dot
	@if command -v dot >/dev/null 2>&1; then \
		dot -Tpng architecture.dot -o architecture.png; \
		echo "Generated architecture.png from architecture.dot"; \
	else \
		echo "Warning: graphviz not installed, skipping diagram generation"; \
		echo "Install with: sudo apt install graphviz"; \
	fi

speakereq2x2.so: speakereq2x2_ladspa.o dsp/biquad.o dsp/filterbank.o dsp/crossbar.o dsp/delay.o utils/ini.o utils/configfile.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

speakereq2x2_ladspa.o: speakereq2x2_ladspa.c speakereq2x2_ladspa.h dsp/biquad.h utils/configfile.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

dsp/%.o: dsp/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o dsp/*.o utils/*.o *.so
	rm -f architecture.png
	@echo "Cleaning Debian build artifacts..."
	rm -rf debian/.debhelper debian/ladspa-speakereq debian/tmp debian/files debian/*.substvars debian/*.debhelper.log debian/debhelper-build-stamp
	rm -f debian/*.log debian/*.debhelper
	rm -f debian/compat

install: install-ladspa install-man

install-ladspa: speakereq2x2.so
	mkdir -p $(LADSPA_INSTALL_DIR)
	cp speakereq2x2.so $(LADSPA_INSTALL_DIR)/
	@echo "Installed speakereq2x2.so to $(LADSPA_INSTALL_DIR)/"

install-man: speakereq2x2.1
	@echo "Installing man page..."
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp speakereq2x2.1 $(DESTDIR)$(PREFIX)/share/man/man1/
	@if command -v gzip >/dev/null 2>&1; then \
		gzip -f $(DESTDIR)$(PREFIX)/share/man/man1/speakereq2x2.1; \
		echo "Installed man page to $(DESTDIR)$(PREFIX)/share/man/man1/speakereq2x2.1.gz"; \
	else \
		echo "Installed man page to $(DESTDIR)$(PREFIX)/share/man/man1/speakereq2x2.1"; \
	fi

install-pipewire:
	mkdir -p /etc/pipewire/pipewire.conf.d
	cp pipewire/90-speakereq.conf /etc/pipewire/pipewire.conf.d/90-speakereq.conf
	@echo "Installed PipeWire config to /etc/pipewire/pipewire.conf.d/90-speakereq.conf"
	@echo "Restart PipeWire and WirePlumber: systemctl --user restart pipewire wireplumber"

uninstall:
	rm -f $(LADSPA_INSTALL_DIR)/speakereq2x2.so
	@echo "Uninstalled SpeakerEQ2x2 LADSPA plugin"

# Debian packaging
deb:
	if [ -n "$$DIST" ]; then \
		echo "Using distribution from DIST environment variable: $$DIST"; \
		DIST_ARG="--dist=$$DIST"; \
		CHROOT_ARG="--chroot=$$CHROOT"; \
	else \
		echo "No DIST environment variable set, using sbuild default"; \
		DIST_ARG=""; \
		CHROOT_ARG=""; \
	fi; \
	sbuild \
		--chroot-mode=unshare \
		--no-clean-source \
		--enable-network \
		$$DIST_ARG \
		$$CHROOT_ARG \
		--verbose

deb-clean:
	@echo "Cleaning Debian build artifacts..."
	rm -rf debian/.debhelper debian/ladspa-speakereq debian/files debian/*.substvars debian/*.debhelper.log
	rm -f ../ladspa-speakereq_*.deb ../ladspa-speakereq_*.buildinfo ../ladspa-speakereq_*.changes
