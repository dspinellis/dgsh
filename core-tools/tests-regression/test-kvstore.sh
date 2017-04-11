#!/bin/sh

DGSH_READVAL=../src/dgsh-readval
DGSH_WRITEVAL=../src/dgsh-writeval
DGSH_HTTPVAL=../src/dgsh-httpval

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
	$DGSH_READVAL -q -s testsocket 2>/dev/null
	if [ "$SERVER_PID" ]
	then
		stop_server
	fi
	echo
	echo Server errors:
	test server.err && cat server.err
	echo Client errors:
	test client.err && cat client.err
	exit 1
}

# Verify a test result
# -n disables the termination of writeval
check()
{
	if [ "$1" != '-n' ]
	then
		$DGSH_READVAL -q -s testsocket 2>/dev/null
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
	rm -f client.err server.err
}

# A particular test case
testcase()
{
	TESTCASE="$1"
	echo -n "	$1: "
}


# Start the HTTP server
start_server()
{
	$DGSH_HTTPVAL -p $PORT "$@" &
	SERVER_PID=$!
}

# Stop the HTTP server
stop_server()
{
	curl -s40 http://localhost:$PORT/.server?quit >/dev/null
	sleep 1
	kill $SERVER_PID 2>/dev/null
	sleep 2
	SERVER_PID=''
}


# Simple tests {{{1
section 'Simple tests' # {{{2

testcase "Single record" # {{{3
echo single record | $DGSH_WRITEVAL -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -l -s testsocket 2>client.err `"
EXPECT='single record'
check

testcase "Single record fixed width" # {{{3
echo -n 0123456789 | $DGSH_WRITEVAL -l 10 -s testsocket 2>server.err &
sleep 1
EXPECT='0123456789'
TRY="`$DGSH_READVAL -l -s testsocket 2>client.err `"
check

testcase "Record separator" # {{{3
echo -n record1:record2 | $DGSH_WRITEVAL -t : -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -l -s testsocket 2>client.err `"
EXPECT='record1:'
check

sleep 1
testcase "HTTP interface - text data" # {{{3
PORT=53843
echo single record | $DGSH_WRITEVAL -s testsocket 2>server.err &
start_server
# -s40: silent, IPv4 HTTP 1.0
TRY="`curl -s40 http://localhost:$PORT/testsocket`"
EXPECT='single record'
check
stop_server

testcase "HTTP interface - non-blocking empty record" # {{{3
PORT=53843
{ sleep 2 ; echo hi ; } | $DGSH_WRITEVAL -s testsocket 2>server.err &
start_server -n
# -s40: silent, IPv4 HTTP 1.0
TRY="`curl -s40 http://localhost:$PORT/testsocket`"
EXPECT=''
check
stop_server

testcase "HTTP interface - non-blocking first record" # {{{3
PORT=53843
echo 'first record' | $DGSH_WRITEVAL -s testsocket 2>server.err &
start_server -n
# -s40: silent, IPv4 HTTP 1.0
sleep 1
TRY="`curl -s40 http://localhost:$PORT/testsocket`"
EXPECT='first record'
check
stop_server

testcase "HTTP interface - non-blocking second record" # {{{3
PORT=53843
( echo 'first record' ; sleep 1 ; echo 'second record' ) |
$DGSH_WRITEVAL -s testsocket 2>server.err &
start_server -n
# -s40: silent, IPv4 HTTP 1.0
sleep 2
TRY="`curl -s40 http://localhost:$PORT/testsocket`"
EXPECT='second record'
check
stop_server

testcase "HTTP interface - binary data" # {{{3
PORT=53843
perl -e 'BEGIN { binmode STDOUT; }
for ($i = 0; $i < 256; $i++) { print chr($i); }' |
$DGSH_WRITEVAL -l 256 -s testsocket 2>server.err &
start_server -m application/octet-stream
# -s40: silent, IPv4 HTTP 1.0
curl -s40 http://localhost:$PORT/testsocket |
perl -e 'BEGIN { binmode STDIN; }
for ($i = 0; $i < 256; $i++) { $expect .= chr($i); }
read STDIN, $try, 256;
if ($try ne $expect) {
	print "FAIL\n";
	print "Expected [$expect] got [$try]\n";
} else {
	print "OK\n";
}
exit ($try ne $expect);
'
EXITCODE=$?
stop_server
$DGSH_READVAL -q -s testsocket 2>client.err 1>/dev/null
test $EXITCODE = 0 || exit 1

testcase "HTTP interface - large data" # {{{3
PORT=53843
dd if=/dev/zero bs=1M count=1 2>/dev/null |
$DGSH_WRITEVAL -l 1000000 -s testsocket 2>server.err &
start_server -m application/octet-stream
# -s40: silent, IPv4 HTTP 1.0
BYTES=$(curl -s40 http://localhost:$PORT/testsocket |
  wc -c |
  sed 's/^[^0-9]*//')
stop_server
$DGSH_READVAL -q -s testsocket 2>client.err 1>/dev/null
if [ "$BYTES" != 1000000 ]
then
	echo FAIL
	echo "Expected [1000000 bytes] got [$BYTES bytes]"
	exit 1
else
	echo "OK"
fi

# Last record tests {{{1
section 'Reading of fixed-length records in stream' # {{{2
(echo -n A12345A7AB; sleep 4; echo -n 12345B7BC; sleep 4; echo -n 12345C7CD) | $DGSH_WRITEVAL -l 9 -s testsocket 2>server.err &

testcase "Record one" # {{{3
sleep 2
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err`"
EXPECT=A12345A7A
check -n

testcase "Record two" # {{{3
sleep 4
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT=B12345B7B
check -n

testcase "Record three" # {{{3
sleep 4
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err`"
EXPECT=C12345C7C
check

# The socket is no longer there
# Verify that readval doesn't block retrying
$DGSH_READVAL -nq -s testsocket 2>client.err


section 'Reading of newline-separated records in stream' # {{{2
(echo record one; sleep 4; echo record two; sleep 4; echo record three) | $DGSH_WRITEVAL -s testsocket 2>server.err &

testcase "Record one" # {{{3
sleep 2
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='record one'
check -n

testcase "Record two" # {{{3
sleep 4
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='record two'
check -n

testcase "Record three" # {{{3
sleep 4
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='record three'
check

section 'Non-blocking reading of newline-separated records in stream' # {{{2
(sleep 2 ; echo record one; sleep 4; echo record two; sleep 6; echo record three) | $DGSH_WRITEVAL -s testsocket 2>server.err &

testcase "Empty record" # {{{3
TRY="`$DGSH_READVAL -e -s testsocket 2>client.err `"
EXPECT=''
check -n

testcase "Record one" # {{{3
sleep 3
TRY="`$DGSH_READVAL -e -s testsocket 2>client.err `"
EXPECT='record one'
check -n

testcase "Record two" # {{{3
sleep 5
TRY="`$DGSH_READVAL -e -s testsocket 2>client.err `"
EXPECT='record two'
check

section 'Reading last record' # {{{2

testcase "Last record" # {{{3
(echo record one; sleep 1; echo last record) | $DGSH_WRITEVAL -s testsocket 2>server.err &
TRY="`$DGSH_READVAL -l -s testsocket 2>client.err `"
EXPECT='last record'
check

testcase "Last record as default" # {{{3
(echo record one; sleep 1; echo last record) | $DGSH_WRITEVAL -s testsocket 2>server.err &
TRY="`$DGSH_READVAL -s testsocket 2>client.err `"
EXPECT='last record'
check

testcase "Empty record" # {{{3
$DGSH_WRITEVAL -s testsocket 2>server.err </dev/null &
TRY="`$DGSH_READVAL -l -s testsocket 2>client.err `"
EXPECT=''
check

testcase "Incomplete record" # {{{3
echo -n unterminated | $DGSH_WRITEVAL -s testsocket 2>server.err &
TRY="`$DGSH_READVAL -l -s testsocket 2>client.err `"
EXPECT=''
check

# Window tests {{{1
section 'Window from stream' # {{{2

testcase "Second record" # {{{3
(echo first record ; echo second record ; echo third record; sleep 2) | $DGSH_WRITEVAL -b 2 -e 1 -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='second record'
check

testcase "First record" # {{{3
(echo first record ; echo second record ; echo third record; sleep 2) | $DGSH_WRITEVAL -b 3 -e 2 -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='first record'
check

testcase "Second record" # {{{3
(echo first record ; echo second record ; echo third record; sleep 2) | $DGSH_WRITEVAL -b 3 -e 1 -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='first record
second record'
check

testcase "All records" # {{{3
(echo first record ; echo second record ; echo third record; sleep 2) | $DGSH_WRITEVAL -b 3 -e 0 -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='first record
second record
third record'
check

section 'Window from fixed record stream' # {{{2

testcase "Middle record" # {{{3
(echo -n 000 ; echo -n 011112 ; echo -n 222; sleep 2) | $DGSH_WRITEVAL -l 4 -b 2 -e 1 -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='1111'
check

testcase "First record" # {{{3
(echo -n 000 ; echo -n 011112 ; echo -n 222; sleep 2) | $DGSH_WRITEVAL -l 4 -b 3 -e 2 -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='0000'
check

testcase "First two records" # {{{3
(echo -n 000 ; echo -n 011112 ; echo -n 222; sleep 2) | $DGSH_WRITEVAL -l 4 -b 3 -e 1 -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='00001111'
check

testcase "All records" # {{{3
(echo -n 000 ; echo -n 011112 ; echo -n 222; sleep 2) | $DGSH_WRITEVAL -l 4 -b 3 -e 0 -s testsocket 2>server.err &
sleep 1
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='000011112222'
check


# Time window tests {{{1
section 'Time window from terminated record stream' # {{{2

testcase "First record" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | $DGSH_WRITEVAL -u s -b 3.5 -e 2.5 -s testsocket 2>server.err &
#Rel  3                             2                               1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='First record'
check

testcase "Middle record" # {{{3
(echo First record ; sleep 1 ; echo Second record ; sleep 1 ; echo The third record; sleep 3) | $DGSH_WRITEVAL -u s -b 2.5 -e 1.5 -s testsocket 2>server.err &
#Rel     3                       2                          1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='Second record'
check

testcase "First two records (full buffer)" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | $DGSH_WRITEVAL -u s -b 3.5 -e 1.5 -s testsocket 2>server.err &
#Rel  3                             2                               1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='First record
Second--record'
check

testcase "First two records (incomplete buffer)" # {{{3
(echo First record ; sleep 1 ; echo Second record ; sleep 1 ; echo The third record; sleep 3) | $DGSH_WRITEVAL -u s -b 3.5 -e 1.5 -s testsocket 2>server.err &
#Rel  3                             2                               1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='First record
Second record'
check

testcase "Last record" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | $DGSH_WRITEVAL -u s -b 1.5 -e 0.5 -s testsocket 2>server.err &
#Rel  3                             2                               1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='The third record'
check

testcase "All records" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | $DGSH_WRITEVAL -u s -b 3.5 -e 0.5 -s testsocket 2>server.err &
#Rel  3                             2                               1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='First record
Second--record
The third record'
check

testcase "First record after wait" # {{{3
(echo First record ; sleep 1 ; echo Second--record ; sleep 1 ; echo The third record; sleep 3) | $DGSH_WRITEVAL -u s -b 4.5 -e 3.5 -s testsocket 2>server.err &
#Rel  3                             2                               1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='First record'
check

section 'Time window from fixed record stream' # {{{2

testcase "Fixed record stream, first record" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 011112 ; sleep 1 ; echo -n 222; sleep 3) | $DGSH_WRITEVAL -l 4 -u s -b 3.5 -e 2.5 -s testsocket 2>server.err &
#Rel     3                       2                          1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='0000'
check

testcase "First two records" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 011112 ; sleep 1 ; echo -n 222; sleep 3) | $DGSH_WRITEVAL -l 4 -u s -b 3.5 -e 1.5 -s testsocket 2>server.err &
#Rel     3                       2                          1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='000011112222'
check

testcase "Middle record" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 011112 ; sleep 1 ; echo -n 222; sleep 3) | $DGSH_WRITEVAL -l 4 -u s -b 2.5 -e 1.5 -s testsocket 2>server.err &
#Rel     3                       2                          1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='11112222'
check

testcase "Last record" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 01111 ; sleep 1 ; echo -n 222; echo -n 2 ; sleep 3) | $DGSH_WRITEVAL -l 4 -u s -b 1.5 -e 0.5 -s testsocket 2>server.err &
#Rel     3                       2                          1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='2222'
check

testcase "All records" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 01111 ; sleep 1 ; echo -n 222; echo -n 2 ; sleep 3) | $DGSH_WRITEVAL -l 4 -u s -b 3.5 -e 0.5 -s testsocket 2>server.err &
#Rel     3                       2                          1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='000011112222'
check


testcase "First record after wait" # {{{3
(echo -n 000 ; sleep 1 ; echo -n 01111 ; sleep 1 ; echo -n 222; echo -n 2 ; sleep 3) | $DGSH_WRITEVAL -l 4 -u s -b 4.5 -e 3.5 -s testsocket 2>server.err &
#Rel     3                       2                          1
sleep 3
TRY="`$DGSH_READVAL -c -s testsocket 2>client.err `"
EXPECT='0000'
check

section 'Multi-client stress test' # {{{1
echo -n "	Running"

if [ ! -r test/sorted-words ]
then
	mkdir -p test
	{
		cat /usr/share/dict/words 2>/dev/null ||
		curl -s https://raw.githubusercontent.com/freebsd/freebsd/master/share/dict/web2
	} | head -50000 | sort >test/sorted-words
fi

$DGSH_WRITEVAL -s testsocket 2>server.err <test/sorted-words &

# Number of parallel clients to run
NUMCLIENTS=5

# Number of read commands to issue
NUMREADS=300

(
	for i in `sequence $NUMCLIENTS`
	do
			for j in `sequence $NUMREADS`
			do
				$DGSH_READVAL -c -s testsocket 2>client.err
			done >read-words-$i &
	done

	wait

	echo
	echo "	All clients finished"
	$DGSH_READVAL -lq -s testsocket 2>client.err 1>/dev/null
	echo "	Server finished"
)

echo -n "	Compare: "

# Wait for the store to terminate
wait

for i in `sequence $NUMCLIENTS`
do
	if sort -u read-words-$i | comm -13 test/sorted-words - | grep .
	then
		fail "Stress test client $i has trash output"
	fi
	if [ `wc -l < read-words-$i` -ne $NUMREADS ]
	then
		fail "Stress test client is missing output"
	fi
done
rm -f read-words-*
echo "OK"

section 'Time window stress test' # {{{1
echo -n "	Running"

# Feed words at a slower pace
perl -pe 'select(undef, undef, undef, 0.001);' test/sorted-words |
$DGSH_WRITEVAL -u s -b 3 -e 2 -s testsocket 2>server.err &
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
				$DGSH_READVAL -c -s testsocket 2>client.err
			done >read-words-$i &
	done

	wait

	echo
	echo "	All clients finished"
	$DGSH_READVAL -q -s testsocket 2>client.err 1>/dev/null
	echo "	Server finished"
)

echo -n "	Compare: "

# Wait for the store to terminate
wait

for i in `sequence $NUMCLIENTS`
do
	if sort -u read-words-$i | comm -13 test/sorted-words - | grep .
	then
		fail "Stress test client $i has trash output"
	fi
	if [ `wc -l < read-words-$i` -lt $NUMREADS ]
	then
		fail "Stress test client may be missing output"
	fi
done

rm -f read-words-* client.err server.err
echo "OK"
