#
# SYNOPSIS Web log reporting
# DESCRIPTION
# Creates a report for a fixed-size web log file read from the standard input.
# Demonstrates the combined use of stores and named streams,
# the use of shell group commands and functions in the scatter block, and
# the use of cat(1) as a way to sequentially combine multiple streams.
# Used to measure throughput increase achieved through parallelism.
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

export SGSH_DOT_DRAW="$(basename $0 .sh).dot"

# Output the top X elements of the input by the number of their occurrences
# X is the first argument
toplist()
{
	uniq -c | sort -rn | head -$1
}

# Output the argument as a section header
header()
{
	echo
	echo "$1"
	echo "$1" | sed 's/./-/g'
}

# Consistent sorting
export LC_ALL=C

export -f toplist
export -f header

#Accesses per day: $(expr $(store:nAccess) / $(store:nDays))
#MBytes per day: $(expr $(store:nXBytes) / $(store:nDays) / 1024 / 1024)

cat <<EOF
			WWW server statistics
			=====================

Summary
-------
EOF

sgsh-tee |
{{

	# Number of transferred bytes
	echo -n 'Number of Gbytes transferred: ' &
	awk '{s += $NF} END {print s / 1024 / 1024 / 1024}' &
	#|store:nXBytes

	# Number of log file bytes
	echo -n 'MBytes log file size: ' &
	wc -c |
	awk '{print $1 / 1024 / 1024}' &
	#|store:nLogBytes

	# Host names
	awk '{print $1}' |
	sgsh-tee |
	{{
		# Number of accesses
		echo -n 'Number of accesses: ' &
		wc -l &
		 #|store:nAccess

		# Sorted hosts
		sort |
		sgsh-tee |
		{{

			# Unique hosts
			uniq |
			sgsh-tee |
			{{
				# Number of hosts
				echo -n 'Number of hosts: ' &
				wc -l &
				#|store:nHosts

				# Number of TLDs
				echo -n 'Number of top level domains: ' &
				awk -F. '$NF !~ /[0-9]/ {print $NF}' |
				sort -u |
				wc -l &
				#|store:nTLD
			}} &

			# Top 10 hosts
			{{
				 call 'header "Top 10 Hosts"' &
				 call 'toplist 10' &
			}} &
			#|>/stream/top10HostsByN
		}} &

		# Top 20 TLDs
		{{
			call 'header "Top 20 Level Domain Accesses"' &
			awk -F. '$NF !~ /^[0-9]/ {print $NF}' |
			sort |
			call 'toplist 20' &
		}} &
		#|>/stream/top20TLD

		# Domains
		awk -F. 'BEGIN {OFS = "."}
		            $NF !~ /^[0-9]/ {$1 = ""; print}' |
		sort |
		sgsh-tee |
		{{
			# Number of domains
			echo -n 'Number of domains: ' &
			uniq |
			wc -l &
			#|store:nDomain

			# Top 10 domains
			{{
				 call 'header "Top 10 Domains"' &
				 call 'toplist 10' &
			}} &
			#|>/stream/top10Domain
		}} &
	}} &

	# Hosts by volume
	{{
		call 'header "Top 10 Hosts by Transfer"' &
		awk '    {bytes[$1] += $NF}
		END {for (h in bytes) print bytes[h], h}' |
		sort -rn |
		head -10 &
	}} &
	#|>/stream/top10HostsByVol

	# Sorted page name requests
	awk '{print $7}' |
	sort |
	sgsh-tee |
	{{

		# Top 20 area requests (input is already sorted)
		{{
			 call 'header "Top 20 Area Requests"' &
			 awk -F/ '{print $2}' |
			 call 'toplist 20' &
		}} &
		#|>/stream/top20Area

		# Number of different pages
		echo -n 'Number of different pages: ' &
		uniq |
		wc -l &
		#|store:nPages

		# Top 20 requests
		{{
			 call 'header "Top 20 Requests"' &
			 call 'toplist 20' &
		}} &
		#|>/stream/top20Request
	}} &

	# Access time: dd/mmm/yyyy:hh:mm:ss
	awk '{print substr($4, 2)}' |
	sgsh-tee |
	{{

		# Just dates
		awk -F: '{print $1}' |
		sgsh-tee |
		{{

			# Number of days
			echo -n 'Number of days: ' &
			uniq |
			wc -l &
			#|store:nDays

			{{
				 call 'header "Accesses by Date"' &
				 uniq -c &
			}} &
			#|>/stream/accessByDate

			# Accesses by day of week
			{{
				 call 'header "Accesses by Day of Week"' &
				 sed 's|/|-|g' |
				 call '(date -f - +%a 2>/dev/null || gdate -f - +%a)' |
				 sort |
				 uniq -c |
				 sort -rn &
			}} &
			#|>/stream/accessByDoW
		}} &

		# Hour
		{{
			call 'header "Accesses by Local Hour"' &
			awk -F: '{print $2}' |
			sort |
			uniq -c &
		}} &
		#|>/stream/accessByHour
	}} &
}} |
sgsh-tee
