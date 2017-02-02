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

UNIX_TOOLS=unix-tools
CORE_TOOLS=core-tools

# Manual pages
MAN1SRC=$(wildcard $(CORE_TOOLS)/src/*.1)
MANPDF=$(patsubst %.1,%.pdf,$(MAN1SRC)) dgsh_negotiate.pdf
MANHTML=$(patsubst %.1,%.html,$(MAN1SRC)) dgsh_negotiate.html

# Web files
EXAMPLES=$(patsubst example/%,%,$(wildcard example/*.sh))
EGPNG=$(patsubst %.sh,png/%-pretty.png,$(EXAMPLES))
WEBPNG=$(EGPNG)
WEBDIST=../../../pubs/web/home/sw/dgsh/

png/%-pretty.png: graphdot/%.dot
	mkdir -p graphdot
	dot $(DOTFLAGS) -Tpng $< >$@

%.pdf: %.1
	groff -man -Tps $< | ps2pdf - $@

%.pdf: $(CORE_TOOLS)/src/%.3
	groff -man -Tps $< | ps2pdf - $@

%.html: %.1
	groff -man -Thtml $< >$@

%.html: $(CORE_TOOLS)/src/%.3
	groff -man -Thtml $< >$@

graphdot/%.dot: example/%.sh
	-DRAW_EXIT=1 DGSH_DOT_DRAW=graphdot/$* ./$(UNIX_TOOLS)/bash/bash --dgsh $< 2>err

all: tools

tools:
	$(MAKE) -C $(CORE_TOOLS) CFLAGS="$(CFLAGS)"
	$(MAKE) -C $(UNIX_TOOLS) make MAKEFLAGS=

export-prefix:
	echo "export PREFIX?=$(PREFIX)" >.config

config: export-prefix config-$(CORE_TOOLS)
	$(MAKE) -C $(UNIX_TOOLS) configure

test-dgsh: $(EXECUTABLES) $(LIBEXECUTABLES)
	./test-dgsh.sh

test-tee: dgsh-tee charcount test-tee.sh
	./test-tee.sh

test: unit-tests test-tools

config-$(CORE_TOOLS): $(CORE_TOOLS)/configure.ac $(CORE_TOOLS)/Makefile.am $(CORE_TOOLS)/src/Makefile.am $(CORE_TOOLS)/tests/Makefile.am
	-mkdir $(CORE_TOOLS)/m4
	cd $(CORE_TOOLS) && \
	autoreconf --install && \
	./configure --prefix=$(PREFIX) \
	--bindir=$(PREFIX)/bin && \
	cd tests && \
	patch Makefile <Makefile.patch

unit-tests:
	cd $(CORE_TOOLS)/tests && \
	$(MAKE) && \
	$(MAKE) check

test-tools:
	$(MAKE) -C $(UNIX_TOOLS) -s test

test-kvstore: test-kvstore.sh
	# Make versions that will exercise the buffers
	$(MAKE) clean
	$(MAKE) DEBUG=1
	./test-kvstore.sh
	# Remove the debug build versions
	$(MAKE) clean

# Regression test based on generated output files
test-regression:
	# Sort files by size to get the easiest problems first
	# Generated dot graphs
	for i in `ls -rS example/*.sh` ; do \
		perl dgsh.pl -g plain $$i >test/regression/graphs/`basename $$i .sh`.test ; \
		diff -b test/regression/graphs/`basename $$i .sh`.* || exit 1 ; \
	done
	# Generated code
	for i in `ls -rS example/*.sh` ; do \
		perl dgsh.pl -o - $$i >test/regression/scripts/`basename $$i .sh`.test ; \
		diff -b test/regression/scripts/`basename $$i .sh`.* || exit 1 ; \
	done
	# Error messages
	for i in test/regression/errors/*.sh ; do \
		! /usr/bin/perl dgsh.pl -o /dev/null $$i 2>test/regression/errors/`basename $$i .sh`.test || exit 1; \
		diff -b test/regression/errors/`basename $$i .sh`.{ok,test} || exit 1 ; \
	done
	# Warning messages
	for i in test/regression/warnings/*.sh ; do \
		/usr/bin/perl dgsh.pl -o /dev/null $$i 2>test/regression/warnings/`basename $$i .sh`.test || exit 1; \
		diff -b test/regression/warnings/`basename $$i .sh`.{ok,test} || exit 1 ; \
	done

# Seed the regression test data
seed-regression:
	for i in example/*.sh ; do \
		echo $$i ; \
		/usr/bin/perl dgsh.pl -o - $$i >test/regression/scripts/`basename $$i .sh`.ok ; \
		/usr/bin/perl dgsh.pl -g plain $$i >test/regression/graphs/`basename $$i .sh`.ok ; \
	done
	for i in test/regression/errors/*.sh ; do \
		echo $$i ; \
		! /usr/bin/perl dgsh.pl -o /dev/null $$i 2>test/regression/errors/`basename $$i .sh`.ok ; \
	done
	for i in test/regression/warnings/*.sh ; do \
		echo $$i ; \
		/usr/bin/perl dgsh.pl -o /dev/null $$i 2>test/regression/warnings/`basename $$i .sh`.ok ; \
	done

clean:
	rm -f *.o *.exe *.a $(MANPDF) $(MANHTML) $(EGPNG)
	$(MAKE) -C $(CORE_TOOLS) clean
	$(MAKE) -C $(UNIX_TOOLS) clean

install:
	$(MAKE) -C $(CORE_TOOLS) install
	$(MAKE) -C $(UNIX_TOOLS) install

webfiles: $(MANPDF) $(MANHTML) $(WEBPNG)

dist: $(MANPDF) $(MANHTML) $(WEBPNG)
	perl -n -e 'if (/^<!-- #!(.*) -->/) { system("$$1"); } else { print; }' web/index.html >$(WEBDIST)/index.html
	cd $(CORE_TOOLS)/src && cp $(MANHTML) $(MANPDF) $(WEBDIST)
	cd $(CORE_TOOLS)/src && cp $(WEBPNG) $(WEBDIST)

pull:
	git pull
	# Reattach detached repositories. These get detached by pulls or
	# by builds specifying a specific gnulib version.
	git submodule status --recursive | awk '{print $$2}' | sort -r | while read d ; do ( cd $$d && git checkout master && git pull ) ; done

commit:
	# Commit -a including submodules with the specified message
	for i in $$(echo $(UNIX_TOOLS)/*/.git | sed 's/\.git//g') . ; do (cd $$i && git commit -am $(MESSAGE) ; done
