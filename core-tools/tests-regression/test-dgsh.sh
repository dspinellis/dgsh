#!/bin/sh
#
# Regression testing of the provided examples
#

TOP=$(cd ../.. ; pwd)
DGSH="$TOP/build/bin/dgsh"
PATH="$TOP/build/bin:$PATH"
export DGSHPATH="$TOP/build/libexec/dgsh"
EXAMPLE="$TOP/example"

# Ensure that the generated test file matches the reference one
# File names are by conventions $base/out.{ok,test}
ensure_same()
{
  local flags=$1
  local base=$2
  echo -n "$base.sh "
  if diff -rw $base/out.ok $base/out.test
  then
    echo OK
  else
    echo "$base.sh: Files differ: $base/out.ok $base/out.test" 1>&2
    exit 1
  fi
}

# Include fallback commands in our executable path
export PATH="$PATH:bin"

LOGFILE=web-log-report/logfile

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


rm -rf */out.test

mkdir -p spell-highlight
echo hello cruwl world | $DGSH $EXAMPLE/spell-highlight.sh >spell-highlight/out.test
ensure_same "$flags" spell-highlight

$DGSH $EXAMPLE/map-hierarchy.sh map-hierarchy/in/a map-hierarchy/in/b map-hierarchy/out.test
ensure_same "$flags" map-hierarchy

$DGSH $EXAMPLE/commit-stats.sh --until '{2013-07-15 23:59 UTC}' >commit-stats/out.test
ensure_same "$flags" commit-stats

$DGSH $EXAMPLE/code-metrics.sh code-metrics/in/ >code-metrics/out.test
ensure_same "$flags" code-metrics

$DGSH $EXAMPLE/duplicate-files.sh duplicate-files >duplicate-files/out.test
ensure_same "$flags" duplicate-files

$DGSH $EXAMPLE/word-properties.sh <word-properties/LostWorldChap1-3 >word-properties/out.test
ensure_same "$flags" word-properties

$DGSH $EXAMPLE/compress-compare.sh <word-properties/LostWorldChap1-3 | sed 's/:.*ASCII.*/: ASCII/;s|/dev/stdin:||' >compress-compare/out.test
ensure_same "$flags" compress-compare

$DGSH $EXAMPLE/web-log-report.sh <web-log-report/logfile >web-log-report/out.test
ensure_same "$flags" web-log-report

(
cd text-properties
rm -rf out.test
mkdir out.test
cd out.test
$DGSH $EXAMPLE/text-properties.sh <../../word-properties/LostWorldChap1-3
)
ensure_same "$flags" text-properties

# Outside the loop, because scatter -s is not compatible with -S
# The correct file was generated using
# tr -s ' \t\n\r\f' \\n <word-properties/LostWorldChap1-3 | sort | uniq -c | sed 's/^  *//'
# An empty line is removed from the test output, because it can be generated
# by tr when the first line of a split file is empty. (In that case \n is not
# a repeated character that tr will remove.)
$DGSH $EXAMPLE/parallel-word-count.sh <word-properties/LostWorldChap1-3 | sed '/^[0-9]* $/d' >parallel-word-count/out.test
ensure_same "" parallel-word-count

exit 0
