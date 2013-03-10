#!/bin/sh


# Helper functions {{{1
# Repeat x times
sequence()
{
	jot $1 2>/dev/null || seq $1 2>/dev/null
}

fail()
{
	echo "FAIL"
	echo "$1"
	./sgsh-readval -q testsocket 2>/dev/null
	exit 1
}

# Verify a test result
# -n disables the termination of writeval
check()
{
	if [ "$1" != '-n' ]
	then
		./sgsh-readval -q testsocket 2>/dev/null
	fi

	if [ "$TRY" != "$EXPECT" ]
	then
		fail "Expected [$EXPECT] got [$TRY]"
	else
		echo "OK"
	fi
}

# A section of the test
section()
{
	SECTION="$1"
	echo "$1"
}

# A particular test case
testcase()
{
	TESTCASE="$1"
	echo -n "	$1: "
}


# Simple tests {{{1
section 'Simple tests' # {{{2

testcase "Single record" # {{{3
echo single record | ./sgsh-writeval testsocket 2>/dev/null &
TRY="`./sgsh-readval -l testsocket 2>/dev/null `"
EXPECT='single record'
check

testcase "Single record fixed width" # {{{3
echo -n 0123456789 | ./sgsh-writeval -l 10 testsocket 2>/dev/null &
EXPECT='0123456789'
TRY="`./sgsh-readval -l testsocket 2>/dev/null `"
check

testcase "Record separator" # {{{3
echo -n record1:record2 | ./sgsh-writeval -t : testsocket 2>/dev/null &
TRY="`./sgsh-readval -l testsocket 2>/dev/null `"
EXPECT='record1:'
check

# Last record tests {{{1
section 'Reading of fixed-length records in stream' # {{{2
(echo -n A12345A7AB; sleep 1; echo -n 12345B7BC; sleep 3; echo -n 12345C7CD) | ./sgsh-writeval -l 9 testsocket 2>/dev/null &

testcase "Record one" # {{{3
TRY="`./sgsh-readval -c testsocket 2>/dev/null`"
EXPECT=A12345A7A
check -n

testcase "Record two" # {{{3
sleep 2
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT=B12345B7B
check -n

testcase "Record three" # {{{3
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null`"
EXPECT=C12345C7C
check

# The socket is no longer there
# Verify that readval doesn't block retrying
./sgsh-readval -nq testsocket 2>/dev/null


section 'Reading of newline-separated records in stream' # {{{2
(echo record one; sleep 1; echo record two; sleep 3; echo record three) | ./sgsh-writeval testsocket 2>/dev/null &

testcase "Record one" # {{{3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='record one'
check -n

testcase "Record two" # {{{3
sleep 2
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='record two'
check -n

testcase "Record three" # {{{3
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='record three'
check

section 'Reading last record' # {{{2

testcase "Last record" # {{{3
(echo record one; sleep 1; echo last record) | ./sgsh-writeval testsocket 2>/dev/null &
TRY="`./sgsh-readval -l testsocket 2>/dev/null `"
EXPECT='last record'
check

testcase "Last record as default" # {{{3
(echo record one; sleep 1; echo last record) | ./sgsh-writeval testsocket 2>/dev/null &
TRY="`./sgsh-readval testsocket 2>/dev/null `"
EXPECT='last record'
check

testcase "Empty record" # {{{3
./sgsh-writeval testsocket 2>/dev/null </dev/null &
TRY="`./sgsh-readval -l testsocket 2>/dev/null `"
EXPECT=''
check

testcase "Incomplete record" # {{{3
echo -n unterminated | ./sgsh-writeval testsocket 2>/dev/null &
TRY="`./sgsh-readval -l testsocket 2>/dev/null `"
EXPECT=''
check

# Window tests {{{1
section 'Window from stream' # {{{2

testcase "Second record" # {{{3
(echo first record ; echo second record ; echo third record; sleep 2) | ./sgsh-writeval -b 2 -e 1 testsocket 2>/dev/null &
sleep 1
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='second record'
check

testcase "First record" # {{{3
(echo first record ; echo second record ; echo third record; sleep 2) | ./sgsh-writeval -b 3 -e 2 testsocket 2>/dev/null &
sleep 1
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='first record'
check

testcase "Second record" # {{{3
(echo first record ; echo second record ; echo third record; sleep 2) | ./sgsh-writeval -b 3 -e 1 testsocket 2>/dev/null &
sleep 1
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='first record
second record'
check

testcase "All records" # {{{3
(echo first record ; echo second record ; echo third record; sleep 2) | ./sgsh-writeval -b 3 -e 0 testsocket 2>/dev/null &
sleep 1
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='first record
second record
third record'
check

section 'Window from fixed record stream' # {{{2

testcase "Middle record" # {{{3
(echo -n 000 ; echo -n 011112 ; echo -n 222; sleep 2) | ./sgsh-writeval -l 4 -b 2 -e 1 testsocket 2>/dev/null &
sleep 1
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='1111'
check

testcase "First record" # {{{3
(echo -n 000 ; echo -n 011112 ; echo -n 222; sleep 2) | ./sgsh-writeval -l 4 -b 3 -e 2 testsocket 2>/dev/null &
sleep 1
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='0000'
check

testcase "First two records" # {{{3
(echo -n 000 ; echo -n 011112 ; echo -n 222; sleep 2) | ./sgsh-writeval -l 4 -b 3 -e 1 testsocket 2>/dev/null &
sleep 1
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='00001111'
check

testcase "All records" # {{{3
(echo -n 000 ; echo -n 011112 ; echo -n 222; sleep 2) | ./sgsh-writeval -l 4 -b 3 -e 0 testsocket 2>/dev/null &
sleep 1
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='000011112222'
check


# Time window tests {{{1
section 'Time window from terminated record stream' # {{{2

testcase "First record" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | ./sgsh-writeval -u s -b 3.5 -e 2.5 testsocket 2>/dev/null &
#Rel  3                             2                               1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='First record'
check

testcase "Middle record" # {{{3
(echo First record ; sleep 1 ; echo Second record ; sleep 1 ; echo The third record; sleep 3) | ./sgsh-writeval -u s -b 2.5 -e 1.5 testsocket 2>/dev/null &
#Rel     3                       2                          1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='Second record'
check

testcase "First two records (full buffer)" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | ./sgsh-writeval -u s -b 3.5 -e 1.5 testsocket 2>/dev/null &
#Rel  3                             2                               1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='First record
Second--record'
check

testcase "First two records (incomplete buffer)" # {{{3
(echo First record ; sleep 1 ; echo Second record ; sleep 1 ; echo The third record; sleep 3) | ./sgsh-writeval -u s -b 3.5 -e 1.5 testsocket 2>/dev/null &
#Rel  3                             2                               1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='First record
Second record'
check

testcase "Last record" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | ./sgsh-writeval -u s -b 1.5 -e 0.5 testsocket 2>/dev/null &
#Rel  3                             2                               1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='The third record'
check

testcase "All records" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | ./sgsh-writeval -u s -b 3.5 -e 0.5 testsocket 2>/dev/null &
#Rel  3                             2                               1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='First record
Second--record
The third record'
check

testcase "First record after wait" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | ./sgsh-writeval -u s -b 4.5 -e 3.5 testsocket 2>/dev/null &
#Rel  3                             2                               1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='First record'
check

section 'Time window from fixed record stream' # {{{2

testcase "Fixed record stream, first record" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 011112 ; sleep 1 ; echo -n 222; sleep 3) | ./sgsh-writeval -l 4 -u s -b 3.5 -e 2.5 testsocket 2>/dev/null &
#Rel     3                       2                          1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='0000'
check

testcase "First two records" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 011112 ; sleep 1 ; echo -n 222; sleep 3) | ./sgsh-writeval -l 4 -u s -b 3.5 -e 1.5 testsocket 2>/dev/null &
#Rel     3                       2                          1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='000011112222'
check

testcase "Middle record" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 011112 ; sleep 1 ; echo -n 222; sleep 3) | ./sgsh-writeval -l 4 -u s -b 2.5 -e 1.5 testsocket 2>/dev/null &
#Rel     3                       2                          1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='11112222'
check

testcase "Last record" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 01111 ; sleep 1 ; echo -n 222; echo -n 2 ; sleep 3) | ./sgsh-writeval -l 4 -u s -b 1.5 -e 0.5 testsocket 2>/dev/null &
#Rel     3                       2                          1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='2222'
check

testcase "All records" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 01111 ; sleep 1 ; echo -n 222; echo -n 2 ; sleep 3) | ./sgsh-writeval -l 4 -u s -b 3.5 -e 0.5 testsocket 2>/dev/null &
#Rel     3                       2                          1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='000011112222'
check


testcase "First record after wait" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 01111 ; sleep 1 ; echo -n 222; echo -n 2 ; sleep 3) | ./sgsh-writeval -l 4 -u s -b 4.5 -e 3.5 testsocket 2>/dev/null &
#Rel     3                       2                          1
sleep 3
TRY="`./sgsh-readval -c testsocket 2>/dev/null `"
EXPECT='0000'
check

section 'Multi-client stress test' # {{{1
echo -n "	Running"
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
				./sgsh-readval -c testsocket 2>/dev/null
			done >read-words-$i &
	done

	wait

	echo
	echo "	All clients finished"
	./sgsh-readval -lq testsocket 2>/dev/null 1>/dev/null
	echo "	Server finished"
)

echo -n "	Compare: "

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
echo "OK"

section 'Time window stress test' # {{{1
echo -n "	Running"
head -50000 /usr/share/dict/words | sort >sorted-words

# Feed words at a slower pace
while read i
do
	echo $i
done <sorted-words |
./sgsh-writeval -u s -b 3 -e 2 testsocket 2>/dev/null &
echo
echo "	Server started"

# Number of parallel clients to run
NUMCLIENTS=5

# Number of read commands to issue
NUMREADS=300

echo -n "	Running clients"
(
	for i in `sequence $NUMCLIENTS`
	do
			for j in `sequence $NUMREADS`
			do
				./sgsh-readval -c testsocket 2>/dev/null
			done >read-words-$i &
	done

	wait

	echo
	echo "	All clients finished"
	./sgsh-readval -q testsocket 2>/dev/null 1>/dev/null
	echo "	Server finished"
)

echo -n "	Compare: "

# Wait for the store to terminate
wait

for i in `sequence $NUMCLIENTS`
do
	if sort -u read-words-$i | comm -13 sorted-words - | grep .
	then
		fail "Stress test client $i has trash output"
	fi
	if [ `wc -l < read-words-$i` -lt $NUMREADS ]
	then
		fail "Stress test client may be missing output"
	fi
done

rm -f sorted-words read-words-*
echo "OK"

