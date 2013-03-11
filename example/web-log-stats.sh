#!/usr/local/bin/sgsh -s /usr/bin/bash
#
# Continuous statistics over web log stream data
# Demonstrates stream processing
# Provide as an argument either the name of a growing web log file
# or -s and a static web log file, which will be processed at a rate
# of about 10 lines per second.
#
#  Copyright 2013 Diomidis Spinellis
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

# Size of the window to report in seconds
WINDOW=10
WINDOW_OLD=$(expr $WINDOW \* 2)

# Update interval in seconds
UPDATE=2

# Print the sum of the numbers read from the standard input
sum()
{
	awk '{ sum += $1 } END {print sum}'
}

# Print the rate of change as a percentage
# between the first (old) and second (new) value
change()
{
	# Can't use bc, because we have numbers in scientific notation
	awk "END {OFMT=\"%.2f%%\"; print ($2 - $1) * 100 / $1}" </dev/null
}

if [ "$1" = "-s" ]
then
	# Simulate log lines coming from a file
	while read line
	do
		echo $line
		sleep 0.01
	done  <"$2"
else
	tail -f "$1"
fi |
scatter |{
	# Window of accessed pages
	-| awk -Winteractive '{print $7}' |store:page -b $WINDOW -u s

	# Get the bytes requested
	-| awk -Winteractive '{print $10}' |{
		# Store total number of bytes
		-| awk -Winteractive '{ s += $1; print s}' |store:total_bytes
		# Store total number of pages requested
		-| awk -Winteractive '{print ++n}' |store:total_pages
		# Window of bytes requested
		-||store:bytes -b $WINDOW -u s
		# Previous window of bytes requested
		-||store:bytes_old -b $WINDOW_OLD -e $WINDOW -u s
	|}

# Gather and print the results
|} gather |{
	# Produce periodic reports
	while :
	do
		WINDOW_PAGES=$(store:bytes -c | wc -l)
		WINDOW_BYTES=$(store:bytes -c | sum )
		WINDOW_PAGES_OLD=$(store:bytes_old -c | wc -l)
		WINDOW_BYTES_OLD=$(store:bytes_old -c | sum)
		clear
		cat <<EOF
Total
-----
Pages: $(store:total_pages -c)
Bytes: $(store:total_bytes -c)

Over last ${WINDOW}s
--------------------
Pages: $WINDOW_PAGES
Bytes: $WINDOW_BYTES
kBytes/s: $(awk "END {OFMT=\"%.0f\"; print $WINDOW_BYTES / $WINDOW / 1000}" </dev/null )
Top page: $(store:page -c | sort | uniq -c | sort -rn | head -1)

Change
------
Requests: $(change $WINDOW_PAGES_OLD $WINDOW_PAGES)
Data bytes: $(change $WINDOW_BYTES_OLD $WINDOW_BYTES)
EOF
	sleep $UPDATE
	done
|}
