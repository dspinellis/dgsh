#!/bin/sh

# Ensure that the files passed as 2nd and 3rd arguments are the same
ensure_same()
{
	echo "$1"
	if ! diff -rb $2 $3 >/dev/null
	then
		echo "$1: $2 and $3 differ" 1>&2
		exit 1
	fi
}

for flags in '' -m -S
do
	echo hello cruwl world | ./sgsh $flags -p . example/spell-highlight.sh >test/spell-highlight/out.test
	ensure_same "$flags spell-highlight.sh" test/spell-highlight/out.ok test/spell-highlight/out.test
	./sgsh $flags -p . example/map-hierarchy.sh test/map-hierarchy/in/a test/map-hierarchy/in/b test/map-hierarchy/out.test
	ensure_same "$flags map-hierarchy.sh" test/map-hierarchy/out.ok test/map-hierarchy/out.test
	./sgsh $flags -p . example/commit-stats.sh --until '{2013-07-15 23:59 UTC}' >test/commit-stats/out.test
	ensure_same "$flags commit-stats.sh" test/commit-stats/out.ok test/commit-stats/out.test
	./sgsh $flags -p . example/code-metrics.sh test/code-metrics/in/ >test/code-metrics/out.test
	ensure_same "$flags code-metrics.sh" test/code-metrics/out.ok test/code-metrics/out.test
	./sgsh $flags -p . example/duplicate-files.sh test/duplicate-files >test/duplicate-files/out.test
	ensure_same duplicate-files.sh test/duplicate-files/out.ok test/duplicate-files/out.test
	./sgsh $flags -p . example/word-properties.sh <test/word-properties/LostWorldChap1-3 >test/word-properties/out.test
	ensure_same "$flags word-properties.sh" test/word-properties/out.ok test/word-properties/out.test
	./sgsh $flags -p . example/compress-compare.sh <test/word-properties/LostWorldChap1-3 | sed 's/:.*ASCII.*/: ASCII/;s|/dev/stdin:||' >test/compress-compare/out.test
	ensure_same "$flags compress-compare.sh" test/compress-compare/out.ok test/compress-compare/out.test
done
