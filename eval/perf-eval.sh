#!/bin/sh
#
# Benchamrk the performance of alternative implementations of the
# provided example files
#
#  Copyright 2014 Diomidis Spinellis
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


mkdir -p time out
rm -f time/*

RESULTS='*.rsf *.vpl Fig *.rsf\@ .sconsign.dbhash hier words.txt trigram.txt digram.txt character.txt'
PERFDIR=`pwd`

# Run the specified command twice, in order to minimize caching effects
run_twice()
{
	local input=$1
	shift

	rm -rf $RESULTS
	"$@" <$input
	rm -rf $RESULTS
	"$@" <$input
}

measure()
{
	local flags=$1
	local script=$2
	local size=$3
	local input=$4
	local name="$PERFDIR/time/$script:$size:$flags"

	shift 4

	echo 1>&2 "Running for $name"

	run_twice $input /usr/bin/time -p -o $name $PERFDIR/../sgsh -p $PERFDIR/.. $flags \
		$PERFDIR/../example/$script "$@"
}

run_examples()
{
	local flags="$1"
	local size=$2
	local text=$3
	local old=$4
	local new=$5
	local weblog=$6
	local git=$7
	local range=$8

	( cd $git ; measure "$flags" commit-stats.sh $size /dev/null ) >out/commit-stats.out

	measure "$flags" spell-highlight.sh $size $text >out/spell.out

	measure "$flags" map-hierarchy.sh $size /dev/null $old $new hier


	measure "$flags" code-metrics.sh $size /dev/null $new >out/metrics.out
	measure "$flags" duplicate-files.sh $size /dev/null $new >out/dup.out
	measure "$flags" word-properties.sh $size $text >out/word-properties.out
	measure "$flags" compress-compare.sh $size $text >out/compress.out
	measure "$flags" web-log-report.sh $size $weblog >out/weblog.out

	measure "$flags" text-properties.sh $size $text
}

# Alternative implementations of (modified) ft2d
/usr/bin/time -p -o time/ft2d.sh:large: ../sgsh -p `pwd`/.. ft2d.sh
/usr/bin/time -p -o time/ft2d.sh:large:-S ../sgsh -p `pwd`/.. -S ft2d.sh

# Compare with native tool
run_twice /dev/null /usr/bin/time -p -o time/ft2d.scons:large: scons

# Alternative implementation of text statistics
run_twice books.txt /usr/bin/time -p -o time/TextProperties:large: java TextProperties

# Alternative implementations of web log statistics
run_twice access.log /usr/bin/time -p -o time/WebStats:large: java WebStats >out/webstats-java.out
run_twice access.log /usr/bin/time -p -o time/web-log-report.pl:large: perl web-log-report.pl >out/webstats-pl.out

# sh and sgsh implementations of other examples
for flags in '' -S
do
	run_examples "$flags" small /dev/null emptydir emptydir access-small.log emptygit HEAD
	run_examples "$flags" large books.txt linux.old linux.new access.log linux $ELOLD..$ELNEW
done
