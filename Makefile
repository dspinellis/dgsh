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

INSTPREFIX?=/usr/local

ifdef DEBUG
CFLAGS=-g -DDEBUG -Wall
else
CFLAGS=-O -Wall
endif

EXECUTABLES=sgsh sgsh-tee sgsh-writeval sgsh-readval sgsh-monitor sgsh-httpval \
	sgsh-ps

# Manual pages
MANSRC=$(wildcard *.1)
MANPDF=$(patsubst %.1,%.pdf,$(MANSRC))
MANHTML=$(patsubst %.1,%.html,$(MANSRC))

# Web files
EXAMPLES=$(patsubst example/%,%,$(wildcard example/*.sh))
WEBPNG=$(patsubst %.sh,png/%-pretty.png,$(EXAMPLES))
WEBDIST=../../../pubs/web/home/sw/sgsh/

%.png: %.sh
	./sgsh -g pretty $< | dot -Tpng >$@

png/%-pretty.png: example/%.sh
	./sgsh -g pretty $< | dot -Tpng >$@

%.pdf: %.1
	groff -man -Tps $< | ps2pdf - $@

%.html: %.1
	groff -man -Thtml $< >$@

all: $(EXECUTABLES)

sgsh-readval: sgsh-readval.c kvstore.c

sgsh-httpval: sgsh-httpval.c kvstore.c

test-sgsh: $(EXECUTABLES)
	./sgsh -p . example/commit-stats.sh --until '{2013-07-15}' >test/commit-stats/out.test
	diff -b test/commit-stats/out.ok test/commit-stats/out.test
	./sgsh -p . example/code-metrics.sh test/code-metrics/in/ >test/code-metrics/out.test
	diff -b test/code-metrics/out.ok test/code-metrics/out.test
	./sgsh -p . example/duplicate-files.sh test/duplicate-files >test/duplicate-files/out.test
	diff test/duplicate-files/out.ok test/duplicate-files/out.test
	./sgsh -p . example/word-properties.sh <test/word-properties/LostWorldChap1-3 >test/word-properties/out.test
	diff -b test/word-properties/out.ok test/word-properties/out.test
	./sgsh -p . example/compress-compare.sh <test/word-properties/LostWorldChap1-3 >test/compress-compare/out.test
	diff -b test/compress-compare/out.ok test/compress-compare/out.test
	./sgsh -m -p . example/commit-stats.sh --until '{2013-07-15}' >test/commit-stats/out.test
	diff -b test/commit-stats/out.ok test/commit-stats/out.test
	./sgsh -m -p . example/code-metrics.sh test/code-metrics/in/ >test/code-metrics/out.test
	diff -b test/code-metrics/out.ok test/code-metrics/out.test
	./sgsh -m -p . example/duplicate-files.sh test/duplicate-files >test/duplicate-files/out.test
	diff test/duplicate-files/out.ok test/duplicate-files/out.test
	./sgsh -m -p . example/word-properties.sh <test/word-properties/LostWorldChap1-3 >test/word-properties/out.test
	diff -b test/word-properties/out.ok test/word-properties/out.test
	./sgsh -m -p . example/compress-compare.sh <test/word-properties/LostWorldChap1-3 >test/compress-compare/out.test
	diff -b test/compress-compare/out.ok test/compress-compare/out.test

test-serial: $(EXECUTABLES)
	./sgsh -S -p . example/code-metrics.sh test/code-metrics/in/ >test/code-metrics/out.test
	diff -b test/code-metrics/out.ok test/code-metrics/out.test
	./sgsh -S -p . example/duplicate-files.sh test/duplicate-files >test/duplicate-files/out.test
	diff test/duplicate-files/out.ok test/duplicate-files/out.test
	./sgsh -S -p . example/word-properties.sh <test/word-properties/LostWorldChap1-3 >test/word-properties/out.test
	diff -b test/word-properties/out.ok test/word-properties/out.test
	./sgsh -S -p . example/compress-compare.sh <test/word-properties/LostWorldChap1-3 >test/compress-compare/out.test
	diff -b test/compress-compare/out.ok test/compress-compare/out.test

test-tee: sgsh-tee charcount test-tee.sh
	./test-tee.sh

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
	perl -c sgsh-ps.pl
	install sgsh-ps.pl sgsh-ps

charcount: charcount.sh
	install charcount.sh charcount

allpng: sgsh
	for i in example/*.sh ; do \
		./sgsh -g pretty $$i | dot -Tpng >png/`basename $$i .sh`-pretty.png ; \
		./sgsh -g pretty-full $$i | dot -Tpng >png/`basename $$i .sh`-pretty-full.png ; \
		./sgsh -g plain $$i | dot -Tpng >png/`basename $$i .sh`-plain.png ; \
	done

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

# Seed the regression test data
seed-regression:
	for i in example/*.sh ; do \
		/usr/bin/perl sgsh.pl -o - $$i >test/regression/scripts/`basename $$i .sh`.ok ; \
		/usr/bin/perl sgsh.pl -g plain $$i >test/regression/graphs/`basename $$i .sh`.ok ; \
	done
	for i in test/regression/errors/*.sh ; do \
		! /usr/bin/perl sgsh.pl -o /dev/null $$i 2>test/regression/errors/`basename $$i .sh`.ok ; \
	done

clean:
	rm -f *.o *.exe $(EXECUTABLES) $(MANPDF) $(MANHTML) $(WEBPNG)

install: $(EXECUTABLES)
	install $(EXECUTABLES) $(INSTPREFIX)/bin
	install -m 644 $(MANSRC) $(INSTPREFIX)/share/man/man1

web: $(MANPDF) $(MANHTML) $(WEBPNG)
	perl -n -e 'if (/^<!-- #!(.*) -->/) { system("$$1"); } else { print; }' index.html >$(WEBDIST)/index.html
	cp $(MANHTML) $(MANPDF) $(WEBDIST)
	cp $(WEBPNG) $(WEBDIST)
