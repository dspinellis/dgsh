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

ifdef DEBUG
CFLAGS=-g -DDEBUG -Wall
else
CFLAGS=-O -Wall
endif

all: sgsh teebuff

test-sgsh: sgsh teebuff
	./sgsh -t ./teebuff example/code-metrics.sh test/code-metrics/in/ >test/code-metrics/out.test
	diff -b test/code-metrics/out.ok test/code-metrics/out.test
	./sgsh -t ./teebuff example/duplicate-files.sh test/duplicate-files/ >test/duplicate-files/out.test
	diff test/duplicate-files/out.ok test/duplicate-files/out.test
	./sgsh -t ./teebuff example/word-properties.sh file://`pwd`/test/word-properties/LostWorldChap1-3 >test/word-properties/out.test
	diff -b test/word-properties/out.ok test/word-properties/out.test

test-teebuff: teebuff charcount
	# Test line scatter reliable algorithm
	cat -n /usr/share/dict/words >words
	./teebuff -sl -b 1000000 <words a b c d
	cat a b c d | sort -n >words2
	diff words words2
	# Test line scatter efficient algorithm
	./teebuff -sl -b 128 <words a b c d
	cat a b c d | sort -n >words2
	diff words words2
	# Test with a buffer smaller than line size
	./teebuff -sl -b 5 <words a b c d
	cat a b c d | sort -n >words2
	diff words words2
	rm words words2
	# Test with data less than the buffer size
	head -50 /usr/share/dict/words | cat -n >words
	./teebuff -sl -b 1000000 <words a b c d
	cat a b c d | sort -n >words2
	diff words words2
	# Test block scatter
	./teebuff -s -b 64 <teebuff.c a b c d
	./charcount <teebuff.c >orig
	cat a b c d | ./charcount >new
	diff orig new
	rm a b c d orig new
	# Test plain distribution
	./teebuff -b 64 <teebuff.c a b
	diff teebuff.c a
	diff teebuff.c b
	rm a b
	# Test output to stdout
	./teebuff -b 64 <teebuff.c >a
	diff teebuff.c a
	rm a

sgsh: sgsh.pl
	perl -c sgsh.pl
	install sgsh.pl sgsh

charcount: charcount.sh
	install charcount.sh charcount
