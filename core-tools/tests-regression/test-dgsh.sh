#!/bin/sh
#
# Regression testing of the provided examples
#

# Ensure that the generated test file matches the reference one
# File names are by conventions test/$base/out.{ok,test}
ensure_same()
{
  local flags=$1
  local base=$2
  echo -n "$base.sh "
  if diff -rw test/$base/out.ok test/$base/out.test >/dev/null
  then
    echo OK
  else
    echo "$base.sh: Files differ: test/$base/out.ok test/$base/out.test" 1>&2
    exit 1
  fi
}

# Include fallback commands in our executable path
export PATH="$PATH:test/bin"

LOGFILE=test/web-log-report/logfile

# Generate a small log file if it is not there
# See http://ita.ee.lbl.gov/html/contrib/ClarkNet-HTTP.html
CLARKNET=ftp://ita.ee.lbl.gov/traces/clarknet_access_log_Aug28.gz
if ! [ -f  $LOGFILE ]
then
	mkdir -p $(dirname $LOGFILE)
	echo "Fetching web test data"
 	{
		curl $CLARKNET 2>/dev/null ||
		wget -O - $CLARKNET 2>/dev/null
	} |
	gzip -dc |
	perl -ne 'print if ($i++ % 1000 == 0)' >$LOGFILE
fi


rm -rf test/*/out.test

mkdir -p test/spell-highlight
echo hello cruwl world | ./dgsh example/spell-highlight.sh >test/spell-highlight/out.test
ensure_same "$flags" spell-highlight

./dgsh example/map-hierarchy.sh test/map-hierarchy/in/a test/map-hierarchy/in/b test/map-hierarchy/out.test
ensure_same "$flags" map-hierarchy

./dgsh example/commit-stats.sh --until '{2013-07-15 23:59 UTC}' >test/commit-stats/out.test
ensure_same "$flags" commit-stats

./dgsh example/code-metrics.sh test/code-metrics/in/ >test/code-metrics/out.test
ensure_same "$flags" code-metrics

./dgsh example/duplicate-files.sh test/duplicate-files >test/duplicate-files/out.test
ensure_same "$flags" duplicate-files

# TODO Fails due to extraneous grep output
#./dgsh example/word-properties.sh <test/word-properties/LostWorldChap1-3 >test/word-properties/out.test
#ensure_same "$flags" word-properties

./dgsh example/compress-compare.sh <test/word-properties/LostWorldChap1-3 | sed 's/:.*ASCII.*/: ASCII/;s|/dev/stdin:||' >test/compress-compare/out.test
ensure_same "$flags" compress-compare

# TODO Order of output different from original (why?); GB wrong
#./dgsh example/web-log-report.sh <test/web-log-report/logfile >test/web-log-report/out.test
#ensure_same "$flags" web-log-report

(
cd test/text-properties
rm -rf out.test
mkdir out.test
cd out.test
../../../dgsh ../../../example/text-properties.sh ../../word-properties/LostWorldChap1-3
)
ensure_same "$flags" text-properties

# Outside the loop, because scatter -s is not compatible with -S
# The correct file was generated using
# tr -s ' \t\n\r\f' \\n <test/word-properties/LostWorldChap1-3 | sort | uniq -c | sed 's/^  *//'
# An empty line is removed from the test output, because it can be generated
# by tr when the first line of a split file is empty. (In that case \n is not
# a repeated character that tr will remove.)
# TODO Make the parallel-word-count work
#./dgsh example/parallel-word-count.sh <test/word-properties/LostWorldChap1-3 | sed '/^[0-9]* $/d' >test/parallel-word-count/out.test
#ensure_same "" parallel-word-count


# TODO
exit
for i in example/*
do
	echo -n "Testing graph of $i "
	if ./dgsh -g pretty -p . $i | fgrep '???' >/dev/null
	then
		echo "FAIL: Graph for $i has unknown nodes" 1>&2
		exit 1
	fi
	echo OK
done
exit 0
