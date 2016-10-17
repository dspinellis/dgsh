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

INSTPREFIX?=/usr/local/sgsh

ifdef DEBUG
CFLAGS=-g -DDEBUG -Wall
else
CFLAGS=-O -Wall
endif

EXECUTABLES=sgsh sgsh-tee sgsh-writeval sgsh-readval sgsh-monitor sgsh-httpval \
	sgsh-ps sgsh-merge-sum sgsh-conc sgsh-wrap

LIBS=libsgsh_negotiate.a

TOOLS=unix-sgsh-tools

# Manual pages
MANSRC=$(wildcard *.1)
MANPDF=$(patsubst %.1,%.pdf,$(MANSRC))
MANHTML=$(patsubst %.1,%.html,$(MANSRC))

# Web files
EXAMPLES=$(patsubst example/%,%,$(wildcard example/*.sh))
EGPNG=$(patsubst %.sh,png/%-pretty.png,$(EXAMPLES))
ENGTPNG=$(patsubst %.sh,png/%-pretty-ngt.png,$(EXAMPLES))
WEBPNG=$(EGPNG) $(ENGTPNG) debug.png profile.png
WEBDIST=../../../pubs/web/home/sw/sgsh/

# Files required for sgsh negotiation
NEGOTIATE_TEST_FILES=sgsh.h sgsh-negotiate.h negotiate.c sgsh-internal-api.h \
		     sgsh-conc.c

png/%-pretty.png: example/%.dot
	dot -Tpng $< >$@

png/%-pretty-ngt.png: example/%-ngt.dot
	dot -Tpng $< >$@

%.pdf: %.1
	groff -man -Tps $< | ps2pdf - $@

%.html: %.1
	groff -man -Thtml $< >$@

all: $(EXECUTABLES) $(LIBS) tools

tools:
	$(MAKE) -C $(TOOLS) make MAKEFLAGS=

config-tools:
	$(MAKE) -C $(TOOLS) configure

sgsh-readval: sgsh-readval.c kvstore.c negotiate.o

sgsh-writeval: sgsh-writeval.c negotiate.o

sgsh-httpval: sgsh-httpval.c kvstore.c

sgsh-conc: sgsh-conc.o negotiate.o

sgsh-wrap: sgsh-wrap.o negotiate.o

sgsh-tee: sgsh-tee.o negotiate.o

test-sgsh: $(EXECUTABLES)
	./test-sgsh.sh

test-tee: sgsh-tee charcount test-tee.sh
	./test-tee.sh

test-merge-sum: sgsh-merge-sum.pl test-merge-sum.sh
	./test-merge-sum.sh

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

sgsh: sgsh.pl
	perl -c sgsh.pl
	install sgsh.pl sgsh

sgsh-ps: sgsh-ps.pl
	! perl -e 'use JSON' 2>/dev/null || perl -c sgsh-ps.pl
	install sgsh-ps.pl sgsh-ps

sgsh-merge-sum: sgsh-merge-sum.pl
	perl -c sgsh-merge-sum.pl
	install sgsh-merge-sum.pl sgsh-merge-sum

libsgsh_negotiate.a: negotiate.c
	ar rcs $@ -o negotiate.o

charcount: charcount.sh
	install charcount.sh charcount

allpng: sgsh
	for i in example/*.sh ; do \
		./sgsh -g pretty $$i | dot -Tpng >png/`basename $$i .sh`-pretty.png ; \
		./sgsh -g pretty-full $$i | dot -Tpng >png/`basename $$i .sh`-pretty-full.png ; \
		./sgsh -g plain $$i | dot -Tpng >png/`basename $$i .sh`-plain.png ; \
	done
	# Outdate example files that need special processing
	touch -r example/ft2d.sh png/ft2d-pretty.png
	touch -r diagram/NMRPipe-pretty-full.dot png/NMRPipe-pretty.png

# Regression test based on generated output files
test-regression:
	# Sort files by size to get the easiest problems first
	# Generated dot graphs
	for i in `ls -rS example/*.sh` ; do \
		perl sgsh.pl -g plain $$i >test/regression/graphs/`basename $$i .sh`.test ; \
		diff -b test/regression/graphs/`basename $$i .sh`.* || exit 1 ; \
	done
	# Generated code
	for i in `ls -rS example/*.sh` ; do \
		perl sgsh.pl -o - $$i >test/regression/scripts/`basename $$i .sh`.test ; \
		diff -b test/regression/scripts/`basename $$i .sh`.* || exit 1 ; \
	done
	# Error messages
	for i in test/regression/errors/*.sh ; do \
		! /usr/bin/perl sgsh.pl -o /dev/null $$i 2>test/regression/errors/`basename $$i .sh`.test || exit 1; \
		diff -b test/regression/errors/`basename $$i .sh`.{ok,test} || exit 1 ; \
	done
	# Warning messages
	for i in test/regression/warnings/*.sh ; do \
		/usr/bin/perl sgsh.pl -o /dev/null $$i 2>test/regression/warnings/`basename $$i .sh`.test || exit 1; \
		diff -b test/regression/warnings/`basename $$i .sh`.{ok,test} || exit 1 ; \
	done

# Seed the regression test data
seed-regression:
	for i in example/*.sh ; do \
		echo $$i ; \
		/usr/bin/perl sgsh.pl -o - $$i >test/regression/scripts/`basename $$i .sh`.ok ; \
		/usr/bin/perl sgsh.pl -g plain $$i >test/regression/graphs/`basename $$i .sh`.ok ; \
	done
	for i in test/regression/errors/*.sh ; do \
		echo $$i ; \
		! /usr/bin/perl sgsh.pl -o /dev/null $$i 2>test/regression/errors/`basename $$i .sh`.ok ; \
	done
	for i in test/regression/warnings/*.sh ; do \
		echo $$i ; \
		/usr/bin/perl sgsh.pl -o /dev/null $$i 2>test/regression/warnings/`basename $$i .sh`.ok ; \
	done

clean: clean-tools
	rm -f *.o *.exe *.a $(EXECUTABLES) $(MANPDF) $(MANHTML) $(EGPNG) $(ENGTPNG)

clean-tools:
	$(MAKE) -C $(TOOLS) clean

install: install-sgsh install-tools

install-sgsh: $(EXECUTABLES) $(LIBS)
	-mkdir -p $(INSTPREFIX)/bin
	-mkdir -p $(INSTPREFIX)/lib
	-mkdir -p $(INSTPREFIX)/share/man/man1
	install $(EXECUTABLES) $(INSTPREFIX)/bin
	install $(LIBS) $(INSTPREFIX)/lib
	install -m 644 $(MANSRC) $(INSTPREFIX)/share/man/man1
	# For tests
	install sgsh-readval /usr/bin

install-tools:
	$(MAKE) -C $(TOOLS) install

web: $(MANPDF) $(MANHTML) $(WEBPNG)
	perl -n -e 'if (/^<!-- #!(.*) -->/) { system("$$1"); } else { print; }' index.html >$(WEBDIST)/index.html
	cp $(MANHTML) $(MANPDF) $(WEBDIST)
	cp $(WEBPNG) $(WEBDIST)

# Debugger examples
debug-word-properties: sgsh
	cat /usr/share/dict/words | ./sgsh -d -p . example/word-properties.sh

debug-web-log-report: sgsh
	gzip -dc eval/clarknet_access_log_Aug28.gz | ./sgsh -d -p . example/web-log-report.sh

# Diagrams that require special processing
png/ft2d-pretty.png: example/ft2d.dot
	dot -Tpng $< | pngtopnm >top.pnm
	cat $< | sed '1,/^}/d' | dot -Tpng | pngtopnm | \
		pnmcat -topbottom top.pnm - | pnmtopng >$@
	rm top.pnm

png/ft2d-pretty-ngt.png: example/ft2d-ngt.dot
	dot -Tpng $< | pngtopnm >top.pnm
	cat $< | sed '1,/^}/d' | dot -Tpng | pngtopnm | \
		pnmcat -topbottom top.pnm - | pnmtopng >$@
	rm top.pnm

#png/NMRPipe-pretty.png: diagram/NMRPipe-pretty-full.dot
#	dot -Tpng $< >png/NMRPipe-pretty.png
