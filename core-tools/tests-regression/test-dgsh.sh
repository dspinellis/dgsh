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
  local base=$1
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
ensure_same spell-highlight

$DGSH $EXAMPLE/map-hierarchy.sh map-hierarchy/in/a map-hierarchy/in/b map-hierarchy/out.test
ensure_same map-hierarchy

if [ -f "$TOP/unix-tools/grep/.git" ] && [ -d "$TOP/.git" ]; then
	(
	cd $TOP/unix-tools/grep
	LC_ALL=C $DGSH $EXAMPLE/commit-stats.sh --since=2010-01-01Z00:00 \
	--until=2015-12-31Z23:59 \
	>$TOP/core-tools/tests-regression/commit-stats/out.test
	)
	ensure_same commit-stats
else
	echo "Skip commit-stats test because input is missing (grep's git repo)"
fi

# This example outputs different result for NMACRO than the template
# due to a variation in the behavior of grep or a variation in the
# implementation of the sgsh example in regression/scripts/code-metrics.ok.
# The test succeeds in Mac OS.
#DGSH_TIMEOUT=20 $DGSH $EXAMPLE/code-metrics.sh code-metrics/in/ >code-metrics/out.test 2>/dev/null
#ensure_same code-metrics

$DGSH $EXAMPLE/duplicate-files.sh duplicate-files >duplicate-files/out.test
ensure_same duplicate-files

$DGSH $EXAMPLE/word-properties.sh <word-properties/LostWorldChap1-3 >word-properties/out.test
ensure_same word-properties

$DGSH $EXAMPLE/compress-compare.sh <word-properties/LostWorldChap1-3 | sed 's/:.*ASCII.*/: ASCII/;s|/dev/stdin:||' >compress-compare/out.test
ensure_same compress-compare

KVSTORE_RETRY_LIMIT=100 DGSH_TIMEOUT=60 $DGSH $EXAMPLE/web-log-report.sh <web-log-report/logfile >web-log-report/out.test
ensure_same web-log-report

(
cd text-properties
rm -rf out.test
mkdir out.test
cd out.test
$DGSH $EXAMPLE/text-properties.sh <../../word-properties/LostWorldChap1-3
)
ensure_same text-properties

# The correct file was generated using
# tr -s ' \t\n\r\f' \\n <word-properties/LostWorldChap1-3 | sort | uniq -c | sed 's/^  *//'
# An empty line is removed from the test output, because it can be generated
# by tr when the first line of a split file is empty. (In that case \n is not
# a repeated character that tr will remove.)
$DGSH $EXAMPLE/parallel-word-count.sh <word-properties/LostWorldChap1-3 | sed '/^[0-9]* $/d' >parallel-word-count/out.test
ensure_same parallel-word-count

$DGSH $EXAMPLE/author-compare.sh conf/icse/ journals/software/ \
  <author-compare/dblp-subset.gz >author-compare/out.test
ensure_same author-compare

exit 0
