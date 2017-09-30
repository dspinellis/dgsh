#!/bin/sh

PSDIR=$1

set -e

./run_simple_test.sh $PSDIR multipipe_one_last \
	'cat /dev/null | {{ cat  & }}'

./run_simple_test.sh $PSDIR multipipe_one_start \
	'{{ cat /dev/null & }} | cat'

CAT=`which cat`
./run_simple_test.sh $PSDIR function_bash_tools \
	"function h
	{
		$CAT | $CAT
	}

	function g
	{
		$CAT | h | $CAT
	}

	$CAT /dev/null | g"

./run_simple_test.sh $PSDIR function_dgsh_tools \
	'function h
	{
		cat | cat
	}

	function g
	{
		cat | h | cat
	}

	cat /dev/null | g'

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

./run_simple_test.sh $PSDIR recursive_multipipe_oneline_start \
	"{{ {{ echo hello ; }} | cat ; echo world ; }} | cat"

./run_simple_test.sh $PSDIR recursive_multipipe_oneline_end \
	"{{ echo world ; {{ echo hello ; }} | cat ; }} | cat"

./run_simple_test.sh $PSDIR nondgsh \
	"true || false
	dgsh-enumerate 2 | cat"

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

# Wrapped diff
./run_simple_test.sh $PSDIR diff4 '! dgsh-enumerate 4 | diff -w --label foo --to-file=/dev/null'
./run_simple_test.sh $PSDIR diff2 '! dgsh-enumerate 2 | diff -w --label foo'
./run_simple_test.sh $PSDIR diff1 '! dgsh-enumerate 1 | diff -w --label foo /dev/null'
./run_simple_test.sh $PSDIR diff1-stdin '! dgsh-enumerate 2 | diff -w --label foo -'
./run_simple_test.sh $PSDIR diff0 'diff -w --label foo /dev/null /dev/null'
./run_simple_test.sh $PSDIR diff0-stdin1 '! dgsh-enumerate 1 | diff -w --label foo - /dev/null'
./run_simple_test.sh $PSDIR diff0-stdin2 '! dgsh-enumerate 1 | diff -w --label foo /dev/null -'
./run_simple_test.sh $PSDIR diff0-noin '! dgsh-enumerate 1 | diff -w --label foo /dev/null /dev/null'

# Wrapped cmp
./run_simple_test.sh $PSDIR cmp2-same '{{ echo a ; echo a ; }} | cmp'
./run_simple_test.sh $PSDIR cmp2-diff '! {{ echo a ; echo b ; }} | cmp -s'
./run_simple_test.sh $PSDIR cmp1-same1 'echo -n | cmp - /dev/null'
./run_simple_test.sh $PSDIR cmp1-same2 'echo -n | cmp /dev/null -'
./run_simple_test.sh $PSDIR cmp0 'cmp /dev/null /dev/null'

# Wrapped diff3
./run_simple_test.sh $PSDIR diff3-3 '{{ echo a ; echo b ; echo c; }} | diff3'
./run_simple_test.sh $PSDIR diff3-2 '{{ echo a ; echo b ; }} | diff3 /dev/null'
./run_simple_test.sh $PSDIR diff3-2-stdin1 '{{ echo a ; echo b ; }} | diff3 - /dev/null'
./run_simple_test.sh $PSDIR diff3-2-stdin2 '{{ echo a ; echo b ; }} | diff3 /dev/null -'
./run_simple_test.sh $PSDIR diff3-1 '{{ echo a ; }} | diff3 /dev/null /dev/null'
./run_simple_test.sh $PSDIR diff3-0 'diff3 /dev/null /dev/null /dev/null'

# grep

# Single arg
./run_simple_test.sh $PSDIR grep-noargs 'echo hi | grep .'
./run_simple_test.sh $PSDIR grep-noargs-cat 'echo hi | grep . | cat'
./run_simple_test.sh $PSDIR grep-matching-lines 'echo hi | grep --matching-lines .'
./run_simple_test.sh $PSDIR grep-matching-lines-cat 'echo hi | grep --matching-lines . | cat'
./run_simple_test.sh $PSDIR grep-f 'echo hi | grep -f <(echo .)'
./run_simple_test.sh $PSDIR grep-f-cat 'echo hi | grep -f <(echo .) | cat'
./run_simple_test.sh $PSDIR grep-c 'echo hi | grep -c . '
./run_simple_test.sh $PSDIR grep-c-cat 'echo hi | grep -c . | cat'
./run_simple_test.sh $PSDIR grep-l 'echo hi | grep -l . '
./run_simple_test.sh $PSDIR grep-l-cat 'echo hi | grep -l . | cat'
./run_simple_test.sh $PSDIR grep-Lcap 'echo hi | grep -L hi'
./run_simple_test.sh $PSDIR grep-Lcap-nomatch 'echo hi | grep -L hello | cat'
./run_simple_test.sh $PSDIR grep-Lcap-cat 'echo hi | grep -L . | cat'
./run_simple_test.sh $PSDIR grep-Lcap-cat-nomatch 'echo hi | grep -L hello | cat'
./run_simple_test.sh $PSDIR grep-o 'echo hi | grep -o hi'
./run_simple_test.sh $PSDIR grep-o-cat 'echo hi | grep -o hi | cat'
./run_simple_test.sh $PSDIR grep-v 'echo hi | grep -v hello | cat'
./run_simple_test.sh $PSDIR grep-v-cat 'echo hi | grep -v hello | cat'

# Double args
./run_simple_test.sh $PSDIR grep-matching-lines-c 'echo hi | grep --matching-lines -c . | cat'
./run_simple_test.sh $PSDIR grep-matching-lines-l 'echo hi | grep --matching-lines -l . | cat'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap 'echo hi | grep --matching-lines -L . | cat'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap-nomatch 'echo hi | grep --matching-lines -L hello | paste'

./run_simple_test.sh $PSDIR grep-c-matching-lines 'echo hi | grep -c --matching-lines . | cat'
./run_simple_test.sh $PSDIR grep-c-l 'echo hi | grep -c -l . | cat'
./run_simple_test.sh $PSDIR grep-c-Lcap 'echo hi | grep -c -L . | paste'
./run_simple_test.sh $PSDIR grep-c-Lcap-nomatch 'echo hi | grep -c -L hello | paste'

./run_simple_test.sh $PSDIR grep-l-matching-lines 'echo hi | grep -l --matching-lines . | cat'
./run_simple_test.sh $PSDIR grep-l-c 'echo hi | grep -l -c . | cat'
./run_simple_test.sh $PSDIR grep-l-Lcap 'echo hi | grep -l -L . | paste'
./run_simple_test.sh $PSDIR grep-l-Lcap-nomatch 'echo hi | grep -l -L hello | paste'

./run_simple_test.sh $PSDIR grep-Lcap-matching-lines 'echo hi | grep -L --matching-lines . | paste'
./run_simple_test.sh $PSDIR grep-Lcap-matching-lines-nomatch 'echo hi | grep -L --matching-lines hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-c 'echo hi | grep -L -c . | cat'
./run_simple_test.sh $PSDIR grep-Lcap-c-nomatch 'echo hi | grep -L -c hello | cat'
./run_simple_test.sh $PSDIR grep-Lcap-l 'echo hi | grep -L -l . | paste'
./run_simple_test.sh $PSDIR grep-Lcap-l-nomatch 'echo hi | grep -L -l hello | paste'

# Triple args
./run_simple_test.sh $PSDIR grep-matching-lines-c-l 'echo hi | grep --matching-lines -c -l hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-c-Lcap 'echo hi | grep --matching-lines -c -L hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-c-Lcap-nomatch 'echo hi | grep --matching-lines -c -L hello | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-l-c 'echo hi | grep --matching-lines -l -c hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-l-Lcap 'echo hi | grep --matching-lines -l -L hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-l-Lcap-nomatch 'echo hi | grep --matching-lines -l -L hello | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap-c 'echo hi | grep --matching-lines -L -c hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap-c-nomatch 'echo hi | grep --matching-lines -L -c hello | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap-l 'echo hi | grep --matching-lines -L -l hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap-l-nomatch 'echo hi | grep --matching-lines -L -l hello | paste'

./run_simple_test.sh $PSDIR grep-c-matching-lines-l 'echo hi | grep -c --matching-lines -l hi | paste'
./run_simple_test.sh $PSDIR grep-c-matching-lines-Lcap 'echo hi | grep -c --matching-lines -L hi | paste'
./run_simple_test.sh $PSDIR grep-c-matching-lines-Lcap-nomatch 'echo hi | grep -c --matching-lines -L hello | paste'
./run_simple_test.sh $PSDIR grep-c-l-matching-lines 'echo hi | grep -c -l --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-c-l-Lcap 'echo hi | grep -c -l -L hi | paste'
./run_simple_test.sh $PSDIR grep-c-l-Lcap-nomatch 'echo hi | grep -c -l -L hello | paste'
./run_simple_test.sh $PSDIR grep-c-Lcap-matching-lines 'echo hi | grep -c -L --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-c-Lcap-matching-lines-nomatch 'echo hi | grep -c -L --matching-lines hello | paste'
./run_simple_test.sh $PSDIR grep-c-Lcap-l 'echo hi | grep -c -L -l hi | paste'
./run_simple_test.sh $PSDIR grep-c-Lcap-l-nomatch 'echo hi | grep -c -L -l hello | paste'

./run_simple_test.sh $PSDIR grep-l-matching-lines-c 'echo hi | grep -l --matching-lines -c hi | paste'
./run_simple_test.sh $PSDIR grep-l-matching-lines-Lcap 'echo hi | grep -l --matching-lines -L hi | paste'
./run_simple_test.sh $PSDIR grep-l-matching-lines-Lcap-nomatch 'echo hi | grep -l --matching-lines -L hello | paste'
./run_simple_test.sh $PSDIR grep-l-c-matching-lines 'echo hi | grep -l -c --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-l-c-Lcap 'echo hi | grep -l -c -L hi | paste'
./run_simple_test.sh $PSDIR grep-l-c-Lcap-nomatch 'echo hi | grep -l -c -L hello | paste'
./run_simple_test.sh $PSDIR grep-l-Lcap-matching-lines 'echo hi | grep -l -L --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-l-Lcap-matching-lines-nomatch 'echo hi | grep -l -L --matching-lines hello | paste'
./run_simple_test.sh $PSDIR grep-l-Lcap-c 'echo hi | grep -l -L -c hi | paste'
./run_simple_test.sh $PSDIR grep-l-Lcap-c-nomatch 'echo hi | grep -l -L -c hello | paste'

./run_simple_test.sh $PSDIR grep-Lcap-matching-lines-c 'echo hi | grep -L --matching-lines -c hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-matching-lines-c-nomatch 'echo hi | grep -L --matching-lines -c hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-matching-lines-l 'echo hi | grep -L --matching-lines -l hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-matching-lines-l-nomatch 'echo hi | grep -L --matching-lines -l hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-c-matching-lines 'echo hi | grep -L -c --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-c-matching-lines-nomatch 'echo hi | grep -L -c --matching-lines hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-c-l 'echo hi | grep -L -c -l hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-c-l-nomatch 'echo hi | grep -L -c -l hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-l-matching-lines 'echo hi | grep -L -l --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-l-matching-lines-nomatch 'echo hi | grep -L -l --matching-lines hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-l-c 'echo hi | grep -L -l -c hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-l-c-nomatch 'echo hi | grep -L -l -c hello | paste'

# Quadraple args
./run_simple_test.sh $PSDIR grep-matching-lines-c-l-Lcap 'echo hi | grep --matching-lines -c -l -L hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-c-Lcap-l 'echo hi | grep --matching-lines -c -L -l hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-l-c-Lcap 'echo hi | grep --matching-lines -l -c -L hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-l-Lcap-c 'echo hi | grep --matching-lines -l -L -c hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap-c-l 'echo hi | grep --matching-lines -L -c -l hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap-l-c 'echo hi | grep --matching-lines -L -l -c hi | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-c-l-Lcap-nomatch 'echo hi | grep --matching-lines -c -l -L hello | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-c-Lcap-l-nomatch 'echo hi | grep --matching-lines -c -L -l hello | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-l-c-Lcap-nomatch 'echo hi | grep --matching-lines -l -c -L hello | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-l-Lcap-c-nomatch 'echo hi | grep --matching-lines -l -L -c hello | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap-c-l-nomatch 'echo hi | grep --matching-lines -L -c -l hello | paste'
./run_simple_test.sh $PSDIR grep-matching-lines-Lcap-l-c-nomatch 'echo hi | grep --matching-lines -L -l -c hello | paste'

./run_simple_test.sh $PSDIR grep-c-matching-lines-l-Lcap 'echo hi | grep -c --matching-lines -l -L hi | paste'
./run_simple_test.sh $PSDIR grep-c-matching-lines-Lcap-l 'echo hi | grep -c --matching-lines -L -l hi | paste'
./run_simple_test.sh $PSDIR grep-c-l-matching-lines-Lcap 'echo hi | grep -c -l --matching-lines -L hi | paste'
./run_simple_test.sh $PSDIR grep-c-l-Lcap-matching-lines 'echo hi | grep -c -l -L --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-c-Lcap-matching-lines-l 'echo hi | grep -c -L --matching-lines -l hi | paste'
./run_simple_test.sh $PSDIR grep-c-Lcap-l-matching-lines 'echo hi | grep -c -L -l --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-c-matching-lines-l-Lcap-nomatch 'echo hi | grep -c --matching-lines -l -L hello | paste'
./run_simple_test.sh $PSDIR grep-c-matching-lines-Lcap-l-nomatch 'echo hi | grep -c --matching-lines -L -l hello | paste'
./run_simple_test.sh $PSDIR grep-c-l-matching-lines-Lcap-nomatch 'echo hi | grep -c -l --matching-lines -L hello | paste'
./run_simple_test.sh $PSDIR grep-c-l-Lcap-matching-lines-nomatch 'echo hi | grep -c -l -L --matching-lines hello | paste'
./run_simple_test.sh $PSDIR grep-c-Lcap-matching-lines-l-nomatch 'echo hi | grep -c -L --matching-lines -l hello | paste'
./run_simple_test.sh $PSDIR grep-c-Lcap-l-matching-lines-nomatch 'echo hi | grep -c -L -l --matching-lines hello | paste'

./run_simple_test.sh $PSDIR grep-l-matching-lines-c-Lcap 'echo hi | grep -l --matching-lines -c -L hi | paste'
./run_simple_test.sh $PSDIR grep-l-matching-lines-Lcap-c 'echo hi | grep -l --matching-lines -L -c hi | paste'
./run_simple_test.sh $PSDIR grep-l-c-matching-lines-Lcap 'echo hi | grep -l -c --matching-lines -L hi | paste'
./run_simple_test.sh $PSDIR grep-l-c-Lcap-matching-lines 'echo hi | grep -l -c -L --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-l-Lcap-matching-lines-c 'echo hi | grep -l -L --matching-lines -c hi | paste'
./run_simple_test.sh $PSDIR grep-l-Lcap-c-matching-lines 'echo hi | grep -l -L -c --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-l-matching-lines-c-Lcap-nomatch 'echo hi | grep -l --matching-lines -c -L hello | paste'
./run_simple_test.sh $PSDIR grep-l-matching-lines-Lcap-c-nomatch 'echo hi | grep -l --matching-lines -L -c hello | paste'
./run_simple_test.sh $PSDIR grep-l-c-matching-lines-Lcap-nomatch 'echo hi | grep -l -c --matching-lines -L hello | paste'
./run_simple_test.sh $PSDIR grep-l-c-Lcap-matching-lines-nomatch 'echo hi | grep -l -c -L --matching-lines hello | paste'
./run_simple_test.sh $PSDIR grep-l-Lcap-matching-lines-c-nomatch 'echo hi | grep -l -L --matching-lines -c hello | paste'
./run_simple_test.sh $PSDIR grep-l-Lcap-c-matching-lines-nomatch 'echo hi | grep -l -L -c --matching-lines hello | paste'

./run_simple_test.sh $PSDIR grep-Lcap-matching-lines-c-l 'echo hi | grep -L --matching-lines -c -l hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-matching-lines-l-c 'echo hi | grep -L --matching-lines -l -c hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-c-matching-lines-l 'echo hi | grep -L -c --matching-lines -l hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-c-l-matching-lines 'echo hi | grep -L -c -l --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-l-matching-lines-c 'echo hi | grep -L -l --matching-lines -c hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-l-c-matching-lines 'echo hi | grep -L -l -c --matching-lines hi | paste'
./run_simple_test.sh $PSDIR grep-Lcap-matching-lines-c-l-nomatch 'echo hi | grep -L --matching-lines -c -l hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-matching-lines-l-c-nomatch 'echo hi | grep -L --matching-lines -l -c hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-c-matching-lines-l-nomatch 'echo hi | grep -L -c --matching-lines -l hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-c-l-matching-lines-nomatch 'echo hi | grep -L -c -l --matching-lines hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-l-matching-lines-c-nomatch 'echo hi | grep -L -l --matching-lines -c hello | paste'
./run_simple_test.sh $PSDIR grep-Lcap-l-c-matching-lines-nomatch 'echo hi | grep -L -l -c --matching-lines hello | paste'

# The remaining (double args) combinations don't work
# does not work: --matching-lines
#echo hi | grep --matching-lines -v hi | paste
#echo hi | grep --matching-lines -v hello | paste
# does not work: -o
#echo hi | grep --matching-lines -o hi | paste

# does not work: -o
#echo hi | grep -c -o hi | cat
#echo hi | grep -c -v hi | paste
# does not work: -c
#echo hi | grep -c -v hello | paste

# does not work: -o
#echo hi | grep -l -o hi | cat
# does not work: -l
#echo hi | grep -l -v hi | cat
#echo hi | grep -l -v hello | cat

# does not work: -o
#echo hi | grep -L -o hi | cat
# does not work: -L
#echo hi | grep -L -v hi | paste
#echo hi | grep -L -v hello | paste

# does not work: --matching-lines
#echo hi | grep -o --matching-lines hi | paste
# does not work: -o
#echo hi | grep -o -c hi | cat
# does not work: -o
#echo hi | grep -o -l hi | paste
# does not work: -o
#echo hi | grep -o -L hi | cat
# does not make sense: -o -v
#echo hi | grep -o -v hi | paste
# does not work: -v
#echo hi | grep -o -v hello | paste

# does not work: --matching-lines
#echo hi | grep -v --matching-lines hi | paste
#echo hi | grep -v --matching-lines hello | paste
#echo hi | grep -v -c hi | paste
# does not work -c
#echo hi | grep -v -c hello | paste
# does not work: -l
#echo hi | grep -v -l hi | paste
#echo hi | grep -v -l hello | paste
# does not work: -L
#echo hi | grep -v -L hi | paste
#echo hi | grep -v -L hello | paste
# does not make sense: -v -o
#echo hi | grep -v -o hi | paste
# does not make sense: -v -o
#echo hi | grep -v -o hello | paste

