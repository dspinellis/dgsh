#
# SYNOPSIS Web log statistics
# DESCRIPTION
# Provides continuous statistics over web log stream data.
# Demonstrates stream processing.
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

export -f sum
export -f change

sgsh-wrap bash -c 'if [ "$1" = "-s" ]
then
	# Simulate log lines coming from a file
	while read line
	do
		echo $line
		sleep 0.01
	done  <"$2"
else
	tail -f "$1"
fi' -- "$1" "$2" |
sgsh-tee |
{{
	# Window of accessed pages
	#|store:page -b $WINDOW -u s
	sgsh-wrap awk -Winteractive '{print $7}' &

	# Get the bytes requested
	sgsh-wrap awk -Winteractive '{print $10}' |
	sgsh-tee |
	{{
		# Store total number of bytes
		sgsh-wrap awk -Winteractive '{ s += $1; print s}' &

		# Store total number of pages requested
		sgsh-wrap awk -Winteractive '{print ++n}' &

		# Window of bytes requested
		#-||store:bytes -b $WINDOW -u s
		sgsh-wrap cat &

		# Previous window of bytes requested
		#-||store:bytes_old -b $WINDOW_OLD -e $WINDOW -u s
		sgsh-wrap cat &
	}} |
	sgsh-tee &
}} | sgsh-tee
# Pseudo-gather
#|
#sgsh-wrap cat |
#sgsh-tee -s |
# Produce periodic reports
#sgsh-wrap bash -c while :
#do
#	WINDOW_PAGES=$(cat bytes | wc -l)
#	WINDOW_BYTES=$(cat bytes | sum )
#	WINDOW_PAGES_OLD=$(cat bytes_old | wc -l)
#	WINDOW_BYTES_OLD=$(cat bytes_old | sum)
#	clear
#	cat <<EOF
#Total
#-----
#Pages: $(cat total_pages)
#Bytes: $(cat total_bytes)

#Over last ${WINDOW}s
#--------------------
#Pages: $WINDOW_PAGES
#Bytes: $WINDOW_BYTES
#kBytes/s: $(awk "END {OFMT=\"%.0f\"; print $WINDOW_BYTES / $WINDOW / 1000}" </dev/null )
#Top page: $(cat page | /usr/bin/sort | uniq -c | /usr/bin/sort -rn | head -1)

#Change
#------
#Requests: $(change $WINDOW_PAGES_OLD $WINDOW_PAGES)
#Data bytes: $(change $WINDOW_BYTES_OLD $WINDOW_BYTES)
#EOF
#	sleep $UPDATE
#done'
