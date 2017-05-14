#!/bin/sh

PSDIR=$1

set -e

./run_simple_test.sh $PSDIR read_while \
	'echo "hi
there" |
	while read X; do echo $X; done'

./run_simple_test.sh $PSDIR subshell \
	"(echo a) |
	cat"

./run_simple_test.sh $PSDIR group \
	"{ echo a ;} |
	cat"

./run_simple_test.sh $PSDIR secho_paste \
	"dgsh-pecho hello |
	paste $PSDIR/world"

./run_simple_test.sh $PSDIR wrap-cat_comm_sort \
	"cat $PSDIR/f1s |
	comm $PSDIR/f2s |
	sort |
	wc -l |
	tr -d \" \""

./run_simple_test.sh $PSDIR comm_sort \
	"comm $PSDIR/f1s $PSDIR/f2s |
	sort |
	wc -l |
	tr -d \" \""

./run_simple_test.sh $PSDIR comm_paste \
	"comm $PSDIR/f1s $PSDIR/f2s |
	paste"

./run_simple_test.sh $PSDIR join_sort \
	"join $PSDIR/f1s $PSDIR/f2s |
	sort"

./run_simple_test.sh $PSDIR paste_diff \
	"paste $PSDIR/f1s $PSDIR/f2s |
	diff $PSDIR/f1s |
	cat"

#./run_simple_test.sh $(DGSHPATH) $PSDIR grep_diff \
#	"grep -v -w match $PSDIR/F $PSDIR/ff \
#	| diff"

./run_simple_test.sh $PSDIR grep_comm \
	"grep -l -L match $PSDIR/ff $PSDIR/F |
	comm |
	paste"

./run_simple_test.sh $PSDIR join_sort_diff \
	"join $PSDIR/f1s $PSDIR/f2s |
	sort |
	diff $PSDIR/f3s |
	cat"

# `date`: Check that command substitution
# does not mess pipe substitution
./run_simple_test.sh $PSDIR secho_secho_fgrep \
	"{{
		dgsh-pecho match
		dgsh-pecho \"not `date`\"
	}} |
	grep -F -h match"

./run_simple_test.sh $PSDIR tee-copy_diff_comm \
	"tee <$PSDIR/hello |
	{{
		diff $PSDIR/world
		comm $PSDIR/hello
	}} |
	cat"

# ditto
#./run_simple_test.sh $(DGSHPATH) $PSDIR grep_diff_comm \
#	"grep -l -L -w -v match $PSDIR/ff $PSDIR/F \
#	| {{ \
#		diff & \
#		comm & \
#	}}"

./run_simple_test.sh $PSDIR comm_paste_join_diff \
	"comm $PSDIR/f4ss $PSDIR/f5ss |
	{{
		paste $PSDIR/p1
		join $PSDIR/j2
		diff $PSDIR/d3
	}} |
	cat"

./run_simple_test.sh $PSDIR sort_sort_comm \
	"{{
		sort $PSDIR/f4s 2>$PSDIR/f4s.errb
		sort $PSDIR/f5s 2>$PSDIR/f5s.errb
	}} |
	comm |
	cat"

./run_simple_test.sh $PSDIR sort_sort_comm_paste_join_diff \
	"{{
		sort $PSDIR/f4s
		sort $PSDIR/f5s
	}} |
	comm |
	{{
		paste $PSDIR/p1
		join $PSDIR/j2
		diff $PSDIR/d3
	}} |
	cat"

