#!/bin/sh
#
# Regression tests for dgsh-tee
#

TOP=$(cd ../.. ; pwd)
DGSH_TEE=$TOP/build/libexec/dgsh/dgsh-tee
DGSH_ENUMERATE=$TOP/build/libexec/dgsh/dgsh-enumerate
DGSH_TEE_C=../src/dgsh-tee.c
WORDS=/usr/share/dict/words
DGSH="$TOP/build/bin/dgsh"
PATH="$TOP/build/bin:$PATH"
export DGSHPATH="$TOP/build/libexec/dgsh"

# Ensure that the files passed as 2nd and 3rd arguments are the same
ensure_same()
{
	echo -n "$1 "
	if ! diff $2 $3 >/dev/null
	then
		echo "$1: $2 and $3 differ" 1>&2
		exit 1
	fi
	echo OK
}

# Produce statistics on the character count of the data received from
# standard input
charcount()
{
  sed 's/./&\
  /g' |
  sort |
  uniq -c
}

# Ensure that the numbers in the files passed as 2nd and 3rd arguments
# about are the same
# Line format:
# Buffers allocated: 1025 Freed: 1024 Maximum allocated: 960
ensure_similar_buffers()
{
	echo -n "$1 "
	for nr in 2 3
	do
		for nf in 3 5 8
		do
			if ! awk "
			function abs(v)
			{
				return (v < 0 ? -v : v)
			}
			NR == $nr {ref = \$$nf}
			NR == $nr + 3 {test = \$$nf}
			END { exit (ref + 0 != 0 && abs((test - ref) / ref * 100) > 10) ? 1 : 0 }" "$2" "$3"
			then
				echo "$1: Line $nr, fields $nf of $2 and $3 differ by more than 10%" 1>&2
				exit 1
			fi
		done
	done
	echo OK
}



for flags in '' -I
do
	# Test two input files
	cat $DGSH_TEE_C $DGSH_TEE_C $WORDS >expected
	$DGSH_TEE $flags -b 64 -i $DGSH_TEE_C -i $DGSH_TEE_C -i $WORDS >a
	ensure_same "Three input $flags" expected a
	rm expected a

	# Test line scatter reliable algorithm (stdin)
	cat -n $WORDS >words
	$DGSH_TEE $flags -s -b 1000000 <words -o a -o b -o c -o d
	cat a b c d | sort -n >words2
	ensure_same "Line scatter reliable stdin $flags" words words2

	# Test line scatter reliable algorithm (pipe)
	cat -n $WORDS |
	$DGSH_TEE $flags -s -b 1000000 -o a -o b -o c -o d
	cat a b c d | sort -n >words2
	ensure_same "Line scatter reliable pipe $flags" words words2


	# Test line scatter reliable algorithm (file argument)
	cat -n $WORDS >words
	$DGSH_TEE $flags -s -b 1000000 -i words -o a -o b -o c -o d
	cat a b c d | sort -n >words2
	ensure_same "Line scatter reliable file $flags" words words2

	# Test scatter to blocking sinks
	cat -n $WORDS >words
	for buffer in 128 1000000
	do
		$DGSH -c "
		dgsh-tee -b $buffer -s -i words |
		dgsh-parallel -n 8 cat |
		sort -mn >a"
		ensure_same "Scatter to blocking sinks $flags -b $buffer" words a
	done
	rm a

	# Test line scatter efficient algorithm
	$DGSH_TEE $flags -s -b 128 <words -o a -o b -o c -o d
	cat a b c d | sort -n >words2
	ensure_same "Line scatter efficient $flags" words words2

	# Test with a buffer smaller than line size
	$DGSH_TEE $flags -s -b 5 <words -o a -o b -o c -o d
	cat a b c d | sort -n >words2
	ensure_same "Small buffer $flags" words words2

	# Test with data less than the buffer size
	head -50 $WORDS | cat -n >words
	$DGSH_TEE $flags -s -b 1000000 <words -o a -o b -o c -o d
	cat a b c d | sort -n >words2
	ensure_same "Large buffer $flags" words words2
	rm words words2

	# Test block scatter
	$DGSH_TEE $flags -s -b 64 <$DGSH_TEE_C -o a -o b -o c -o d
	charcount <$DGSH_TEE_C >orig
	cat a b c d | charcount >new
	ensure_same "Block scatter $flags" orig new
	rm a b c d orig new

	# Test plain distribution
	$DGSH_TEE $flags -b 64 <$DGSH_TEE_C -o a -o b
	ensure_same "Plain distribution $flags" $DGSH_TEE_C a
	ensure_same "Plain distribution $flags" $DGSH_TEE_C b
	rm a b

	# Test 2->4 distribution
	$DGSH_TEE $flags -b 64 -i $WORDS -i $DGSH_TEE_C -o a -o b -o c -o d
	ensure_same "2->4 distribution $flags" $WORDS a
	ensure_same "2->4 distribution $flags" $WORDS c
	ensure_same "2->4 distribution $flags" $DGSH_TEE_C b
	ensure_same "2->4 distribution $flags" $DGSH_TEE_C d
	rm a b c d

	# Test 4->2 distribution
	cat $WORDS /etc/services >result1
	cat $DGSH_TEE_C /etc/hosts >result2
	$DGSH_TEE $flags -b 64 -i $WORDS -i $DGSH_TEE_C -i /etc/services -i /etc/hosts -o a -o b
	ensure_same "4->2 distribution $flags" result1 a
	ensure_same "4->2 distribution $flags" result2 b
	rm a b result1 result2

	# Test permutation
	$DGSH -c "$DGSH_ENUMERATE 4 | $DGSH_TEE -p 4,2,3,1 | $DGSH_TEE" >a
	ensure_same "Permutation $flags" a tee/perm.ok
	rm a

	# Test output to stdout
	$DGSH_TEE $flags -b 64 <$DGSH_TEE_C >a
	ensure_same "Stdout $flags" $DGSH_TEE_C a
	rm a

	# Test buffering
	# When -l is supported add, say, -l 16
	for flags2 in '' '-m 2k' '-m 2k -f'
	do
		test="tee-fastout$flags$flags2"
		dd bs=1k count=1024 if=/dev/zero 2>/dev/null | $DGSH_TEE -M $flags $flags2 -b 1024 >/dev/null 2>"tee/$test.test"
		ensure_similar_buffers "$test" "tee/$test.ok" "tee/$test.test"
		test="tee-lagout$flags$flags2"
		dd bs=1k count=1024 if=/dev/zero 2>/dev/null | $DGSH_TEE -M $flags $flags2 -b 1024 2>"tee/$test.test" | (sleep 1 ; cat >/dev/null)
		ensure_similar_buffers "$test" "tee/$test.ok" "tee/$test.test"
	done

	# Test low-memory behavior (memory)
	rm -f try try2
	mkfifo try try2
	perl -e 'for ($i = 0; $i < 500; $i++) { print "x" x 500, "\n"}' | tee lines | $DGSH_TEE $flags -b 512 -m 2k -o try -o try2 2>err &
	cat try2 >try2.out &
	{ read x ; echo $x ; sleep 1 ; cat ; } < try > try.out &
	wait
	if [ "$flags" = '-I' ]
	then
		ensure_same "Low-memory $flags error" err tee/oom.err
	else
		ensure_same "Low-memory (try) $flags" lines try.out
		ensure_same "Low-memory (try2) $flags" lines try2.out
	fi
	rm -f lines try try2 try.out try2.out err

	# Test low-memory behavior (file)
	rm -f try try2
	mkfifo try try2
	perl -e 'for ($i = 0; $i < 500; $i++) { print "x" x 500, "\n"}' | tee lines | DGSH_DEBUG_LEVEL=4 $DGSH_TEE -f $flags -b 512 -m 2k -o try -o try2 2>err &
	cat try2 >try2.out &
	{ read x ; echo $x ; sleep 1 ; cat ; } < try > try.out &
	wait
	ensure_same "Low-memory temporary file (try) $flags" lines try.out
	ensure_same "Low-memory temporary file (try2) $flags" lines try2.out
	rm -f lines try try2 try.out try2.out err
done

# Test asynchronous reading from multiple input files
# Without it the following blocks
# Also test multiple temporary files
for flags in '' '-m 65536 -f'
do
	rm -f a b c d
	mkfifo a b c
	$DGSH_TEE -b 4096 -m 65536 -i /usr/share/dict/words -o a -o b -o c &
	$DGSH_TEE -b 4096 -I $flags -i a -i b -i c -o d &
	wait
        for i in . . .
        do
                cat /usr/share/dict/words
        done >expect
	ensure_same "Asynchronous gather $flags" expect d
	rm -f a b c d expect
done

exit 0
