#!/bin/sh
#
# Regression tests for teebuff
#

ensure_same()
{
	echo "$1"
	if ! diff $2 $3 >/dev/null
	then
		echo "$1: $2 and $3 differ" 1>&2
		exit 1
	fi
}


for flags in '' -i
do
	# Test line scatter reliable algorithm
	cat -n /usr/share/dict/words >words
	./teebuff $flags -sl -b 1000000 <words a b c d
	cat a b c d | sort -n >words2
	ensure_same "Line scatter reliable $flags" words words2

	# Test line scatter efficient algorithm
	./teebuff $flags -sl -b 128 <words a b c d
	cat a b c d | sort -n >words2
	ensure_same "Line scatter efficient $flags" words words2

	# Test with a buffer smaller than line size
	./teebuff $flags -sl -b 5 <words a b c d
	cat a b c d | sort -n >words2
	ensure_same "Small buffer $flags" words words2
	rm words words2

	# Test with data less than the buffer size
	head -50 /usr/share/dict/words | cat -n >words
	./teebuff $flags -sl -b 1000000 <words a b c d
	cat a b c d | sort -n >words2
	ensure_same "Large buffer $flags" words words2

	# Test block scatter
	./teebuff $flags -s -b 64 <teebuff.c a b c d
	./charcount <teebuff.c >orig
	cat a b c d | ./charcount >new
	ensure_same "Block scatter $flags" orig new
	rm a b c d orig new

	# Test plain distribution
	./teebuff $flags -b 64 <teebuff.c a b
	ensure_same "Plain distribution $flags" teebuff.c a
	ensure_same "Plain distribution $flags" teebuff.c b
	rm a b

	# Test output to stdout
	./teebuff $flags -b 64 <teebuff.c >a
	ensure_same "Stdout $flags" teebuff.c a
	rm a
done
exit 0
