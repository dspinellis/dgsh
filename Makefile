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

EXECUTABLES=dgsh-monitor dgsh-httpval dgsh dgsh-readval

LIBEXECUTABLES=dgsh-tee dgsh-parallel dgsh-writeval dgsh-readval dgsh-monitor \
	dgsh-conc dgsh-wrap perm dgsh-merge-sum

LIBS=libdgsh_negotiate.a

TOOLS=unix-dgsh-tools

# Manual pages
MANSRC=$(wildcard *.1)
MANPDF=$(patsubst %.1,%.pdf,$(MANSRC))
MANHTML=$(patsubst %.1,%.html,$(MANSRC))

# Web files
EXAMPLES=$(patsubst example/%,%,$(wildcard example/*.sh))
EGPNG=$(patsubst %.sh,png/%-pretty.png,$(EXAMPLES))
WEBPNG=$(EGPNG)
WEBDIST=../../../pubs/web/home/sw/dgsh/

# Files required for dgsh negotiation
NEGOTIATE_TEST_FILES=dgsh.h dgsh-negotiate.h negotiate.c dgsh-internal-api.h \
		     dgsh-conc.c

png/%-pretty.png: graphdot/%.dot
	mkdir -p graphdot
	dot $(DOTFLAGS) -Tpng $< >$@


%.pdf: %.1
	groff -man -Tps $< | ps2pdf - $@

%.html: %.1
	groff -man -Thtml $< >$@

graphdot/%.dot: example/%.sh
	DRAW_EXIT=1 DGSH_DOT_DRAW=graphdot/$* ./unix-dgsh-tools/bash/bash --dgsh $< </dev/null

all: $(EXECUTABLES) $(LIBEXECUTABLES) $(LIBS) tools

tools:
	$(MAKE) -C $(TOOLS) make MAKEFLAGS=

config:
	echo "export PREFIX?=$(PREFIX)" >.config
	$(MAKE) -C $(TOOLS) configure

dgsh-readval: dgsh-readval.c kvstore.c negotiate.o

dgsh-writeval: dgsh-writeval.c negotiate.o

dgsh-httpval: dgsh-httpval.c kvstore.c

dgsh-conc: dgsh-conc.o negotiate.o

dgsh-wrap: dgsh-wrap.o negotiate.o

dgsh-tee: dgsh-tee.o negotiate.o

dgsh-parallel: dgsh-parallel.sh

dgsh: dgsh.sh
	./replace-paths.sh <$? >$@
	chmod 755 $@

perm: perm.sh
	./replace-paths.sh <$? >$@
	chmod 755 $@

dgsh-merge-sum: dgsh-merge-sum.pl
	./replace-paths.sh <$? >$@
	chmod 755 $@

test-dgsh: $(EXECUTABLES) $(LIBEXECUTABLES)
	./test-dgsh.sh

test-tee: dgsh-tee charcount test-tee.sh
	./test-tee.sh

test-negotiate: copy_files build-run-ng-tests test-tools

setup-test-negotiate: copy_files autoreconf-ng-tests

copy_files: $(NEGOTIATE_TEST_FILES) test/negotiate/tests/check_negotiate.c
	cp $(NEGOTIATE_TEST_FILES) test/negotiate/src/

autoreconf-ng-tests: test/negotiate/configure.ac test/negotiate/Makefile.am test/negotiate/src/Makefile.am test/negotiate/tests/Makefile.am
	-mkdir test/negotiate/m4
	cd test/negotiate && \
	autoreconf --install && \
	./configure && \
	cd tests && \
	patch Makefile <Makefile.patch

build-run-ng-tests:
	cd test/negotiate && \
	$(MAKE) && \
	$(MAKE) check

test-tools:
	$(MAKE) -C $(TOOLS) -s test

test-kvstore: test-kvstore.sh
	# Make versions that will exercise the buffers
	$(MAKE) clean
	$(MAKE) DEBUG=1
	./test-kvstore.sh
	# Remove the debug build versions
	$(MAKE) clean

libdgsh_negotiate.a: negotiate.c
	ar rcs $@ negotiate.o

charcount: charcount.sh
	install charcount.sh charcount

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

clean: clean-dgsh clean-tools

clean-dgsh:
	rm -f *.o *.exe *.a $(EXECUTABLES) $(LIBEXECUTABLES) $(MANPDF) \
		$(MANHTML) $(EGPNG)

clean-tools:
	$(MAKE) -C $(TOOLS) clean

install: install-dgsh install-tools

install-dgsh: $(EXECUTABLES) $(LIBEXECUTABLES) $(LIBS)
	-mkdir -p $(DESTDIR)$(PREFIX)/bin
	-mkdir -p $(DESTDIR)$(PREFIX)/lib
	-mkdir -p $(DESTDIR)$(PREFIX)/libexec/dgsh
	-mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	install $(EXECUTABLES) $(DESTDIR)$(PREFIX)/bin
	install $(LIBEXECUTABLES) $(DESTDIR)$(PREFIX)/libexec/dgsh
	install $(LIBS) $(DESTDIR)$(PREFIX)/lib
	install -m 644 $(MANSRC) $(DESTDIR)$(PREFIX)/share/man/man1

install-tools:
	$(MAKE) -C $(TOOLS) install

web: $(MANPDF) $(MANHTML) $(WEBPNG)
	perl -n -e 'if (/^<!-- #!(.*) -->/) { system("$$1"); } else { print; }' index.html >$(WEBDIST)/index.html
	cp $(MANHTML) $(MANPDF) $(WEBDIST)
	cp $(WEBPNG) $(WEBDIST)

pull:
	git pull
	# Reattach detached repositories. These get detached by pulls or
	# by builds specifying a specific gnulib version.
	git submodule status --recursive | awk '{print $$2}' | sort -r | while read d ; do ( cd $$d && git checkout master && git pull ) ; done
