#
#  Copyright 2012-2013 Diomidis Spinellis
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

-include .config
export PREFIX?=/usr/local

ifdef DEBUG
CFLAGS=-g -DDEBUG -Wall
else
CFLAGS=-O -Wall
endif

ifdef TIME
CFLAGS+=-DTIME
endif

DOTFLAGS=-Nfontname=Arial -Ngradientangle=90 -Nstyle=filled -Nshape=ellipse -Nfillcolor=yellow:white

# Manual pages
MAN1SRC=$(wildcard core-tools/src/*.1)
MANPDF=$(patsubst %.1,%.pdf,$(MAN1SRC)) core-tools/src/dgsh_negotiate.pdf
MANHTML=$(patsubst %.1,%.html,$(MAN1SRC)) core-tools/src/dgsh_negotiate.html

# Web files
EXAMPLES=$(patsubst example/%,%,$(wildcard example/*.sh))
EGPNG=$(patsubst %.sh,png/%-pretty.png,$(EXAMPLES))
WEBPNG=$(EGPNG)
WEBDIST=../../../pubs/web/home/sw/dgsh/

png/%-pretty.png: graphdot/%.dot
	dot $(DOTFLAGS) -Tpng $< >$@

%.pdf: %.1
	groff -man -Tps $< | ps2pdf - $@

%.pdf: %.3
	groff -man -Tps $< | ps2pdf - $@

%.html: %.1
	groff -man -Thtml $< >$@

%.html: %.3
	groff -man -Thtml $< >$@

graphdot/%.dot: example/%.sh
	mkdir -p graphdot
	-DRAW_EXIT=1 DGSH_DOT_DRAW=graphdot/$* ./unix-tools/bash/bash --dgsh $< 2>err

.PHONY: all tools core-tools unix-tools export-prefix \
	config config-core-tools \
	test test-dgsh test-merge-sum test-tee test-negotiate \
	test-unix-tools test-kvstore \
	clean install webfiles dist pull commit uninstall

all: tools

tools: core-tools unix-tools

core-tools:
	$(MAKE) -C core-tools CFLAGS="$(CFLAGS)"
	cd core-tools/src && $(MAKE) build-install

unix-tools:
	$(MAKE) -C unix-tools make MAKEFLAGS=
	$(MAKE) -C unix-tools build-install

export-prefix:
	echo "export PREFIX?=$(PREFIX)" >.config

config: export-prefix config-core-tools
	$(MAKE) -C unix-tools configure

config-core-tools: core-tools/configure.ac core-tools/Makefile.am core-tools/src/Makefile.am core-tools/tests/Makefile.am
	-mkdir core-tools/m4
	cd core-tools && \
	autoreconf --install && \
	./configure --prefix=$(PREFIX) \
	--bindir=$(PREFIX)/bin && \
	cd tests && \
	patch Makefile <Makefile.patch

test: test-negotiate test-tee test-kvstore test-unix-tools test-merge-sum test-dgsh

test-dgsh: tools
	cd core-tools/tests-regression && ./test-dgsh.sh

test-merge-sum:
	cd core-tools/tests-regression && ./test-merge-sum.sh

test-tee: tools
	cd core-tools/tests-regression && ./test-tee.sh

test-negotiate: tools
	cd core-tools/tests && \
	$(MAKE) && \
	$(MAKE) check

test-unix-tools: tools
	$(MAKE) -C unix-tools -s test

test-kvstore:
	# Make versions that will exercise the buffers
	$(MAKE) -C core-tools/src clean
	$(MAKE) -C core-tools/src
	rm core-tools/src/dgsh-writeval.o
	$(MAKE) -C core-tools/src CFLAGS=-DDEBUG
	cd core-tools/tests-regression && ./test-kvstore.sh
	# Remove the debug build versions
	$(MAKE) -C core-tools/src clean
	$(MAKE) -C core-tools/src

clean:
	rm -f *.o *.exe *.a $(MANPDF) $(MANHTML) $(EGPNG)
	$(MAKE) -C core-tools clean
	$(MAKE) -C unix-tools clean

install:
	-rm -r build
	$(MAKE) -C core-tools install
	$(MAKE) -C unix-tools install

webfiles: $(MANPDF) $(MANHTML) $(WEBPNG)

dist: $(MANPDF) $(MANHTML) $(WEBPNG)
	perl -n -e 'if (/^<!-- #!(.*) -->/) { system("$$1"); } else { print; }' web/index.html >$(WEBDIST)/index.html
	cp $(MANHTML) $(MANPDF) $(WEBDIST)
	cp $(WEBPNG) $(WEBDIST)

pull:
	git pull
	# Reattach detached repositories. These get detached by pulls or
	# by builds specifying a specific gnulib version.
	git submodule status --recursive | awk '{print $$2}' | sort -r | while read d ; do ( cd $$d && old=$$(git rev-parse HEAD) && git checkout master && git pull && git checkout $$old ) ; done

commit:
	# Commit -a including submodules with the specified message
	for i in $$(echo unix-tools/*/.git | sed 's/\.git//g') . ; do (cd $$i && git commit -am $(MESSAGE) ; done

# Rough uninstall rule to verify that tests pick up correct files
uninstall:
	rm -rf $(PREFIX)/bin/dgsh-* $(PREFIX)/libexec/dgsh \
		$(PREFIX)/lib/libdgsh.a
