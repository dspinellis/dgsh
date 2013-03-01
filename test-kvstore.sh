#!/bin/sh

# Make versions that will exercise the buffers
make clean
make DEBUG=1

# Repeat x times
sequence()
{
	jot $1 2>/dev/null || seq $1 2>/dev/null
}

# Fail with an error message
fail()
{
	echo "Fail: $1" 1>&2
	./sgsh-readval -q testsocket 2>/dev/null
	exit 1
}

# A section of the test
section()
{
	echo "Running $1"
}

section 'test single result'
echo single record | ./sgsh-writeval testsocket 2>/dev/null &
if test "`./sgsh-readval -lq testsocket 2>/dev/null`" != 'single record'
then
	fail "Single record"
fi

section 'test single result fixed width'
echo -n 0123456789 | ./sgsh-writeval -l 10 testsocket 2>/dev/null &
if test "`./sgsh-readval -lq testsocket 2>/dev/null`" != '0123456789'
then
	fail "Single record fixed width"
fi

section 'test record separator'
echo -n record1:record2 | ./sgsh-writeval -t : testsocket 2>/dev/null &
if test "`./sgsh-readval -lq testsocket 2>/dev/null`" != 'record1:'
then
	fail "Record separator"
fi

section 'test reading of fixed-length records in stream'
(echo -n A12345A7AB; sleep 1; echo -n 12345B7BC; sleep 3; echo -n 12345C7CD) | ./sgsh-writeval -l 9 testsocket 2>/dev/null &

TRY="`./sgsh-readval testsocket 2>/dev/null`"
if test "$TRY" != A12345A7A
then
	fail "Record one [$TRY]"
fi

sleep 2
if test "`./sgsh-readval -c testsocket 2>/dev/null`" != B12345B7B
then
	fail "Record two"
fi

sleep 3
TRY="`./sgsh-readval testsocket 2>/dev/null`"
if test "$TRY" != C12345C7C
then
	fail "Record three: got [$TRY]"
fi

./sgsh-readval -q testsocket 2>/dev/null


section 'test reading of newline-separated records in stream'
(echo record one; sleep 1; echo record two; sleep 3; echo record three) | ./sgsh-writeval testsocket 2>/dev/null &

if test "`./sgsh-readval testsocket 2>/dev/null`" != 'record one'
then
	fail "Record one"
fi

sleep 2
if test "`./sgsh-readval -c testsocket 2>/dev/null`" != 'record two'
then
	fail "Record two"
fi

sleep 3
if test "`./sgsh-readval testsocket 2>/dev/null`" != 'record three'
then
	fail "Record three"
fi

./sgsh-readval -q testsocket 2>/dev/null

section 'test reading last record'
(echo record one; sleep 1; echo last record) | ./sgsh-writeval testsocket 2>/dev/null &
if test "`./sgsh-readval -ql testsocket 2>/dev/null`" != 'last record'
then
	fail "Last record"
fi

section 'test reading empty record'
./sgsh-writeval testsocket 2>/dev/null </dev/null &
if test "`./sgsh-readval -ql testsocket 2>/dev/null`" != ''
then
	fail "Empty record"
fi

section 'test reading incomplete record'
echo -n unterminated | ./sgsh-writeval testsocket 2>/dev/null &
if test "`./sgsh-readval -ql testsocket 2>/dev/null`" != 'unterminated'
then
	fail "Incomplete record"
fi

section 'stress test'
head -50000 /usr/share/dict/words | sort >sorted-words
./sgsh-writeval testsocket 2>/dev/null <sorted-words &

# Number of parallel clients to run
NUMCLIENTS=5

# Number of read commands to issue
NUMREADS=300

(
	for i in `sequence $NUMCLIENTS`
	do
			for j in `sequence $NUMREADS`
			do
				./sgsh-readval testsocket 2>/dev/null
			done >read-words-$i &
	done

	wait

	echo "-All clients finished"
	./sgsh-readval -lq testsocket 2>/dev/null 1>/dev/null
	echo "-Server finished"
)

echo "-Starting compare"

# Wait for the store to terminate
wait

for i in `sequence $NUMCLIENTS`
do
	if sort -u read-words-$i | comm -13 sorted-words - | grep .
	then
		fail "Stress test client $i has trash output"
	fi
	if [ `wc -l < read-words-$i` -ne $NUMREADS ]
	then
		fail "Stress test client is missing output"
	fi
done
rm -f sorted-words read-words-*
echo "-Done"

# Remove the debug build versions
make clean
