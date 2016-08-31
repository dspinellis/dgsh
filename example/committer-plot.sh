#
# SYNOPSIS Plot git committer activity over time
# DESCRIPTION
# Process the git history, and create two PNG diagrams depicting
# committer activity over time. The most active committers appear
# at the center vertical of the diagram.
# Demonstrates image processing, mixining of synchronous and
# asynchronous processing in a scatter block, and the use of an
# sgsh-compliant join command.
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


# Commit history in the form of ascending Unix timestamps, emails
sgsh-wrap git log --pretty=tformat:'%at %ae' |
sgsh-wrap awk 'NF == 2 && $1 > 100000 && $1 < '`date +%s` |
sort -n |
sgsh-tee |
{{
	{{
		# Calculate number of committers
		sgsh-wrap awk '"'"'{print $2}'"'"' |
		sort -u |
		sgsh-wrap -nooutput bash -c 'wc -l >committers' &

		# Calculate number of days in window
		sgsh-wrap tail -1 |
		sgsh-wrap -nooutput bash -c 'awk '"'"'{print $1}'"'"' >last' &

		sgsh-wrap head -1 |
		sgsh-wrap -nooutput bash -c 'awk '"'"'{print $1}'"'"' >first' &
	}} &
	#NCOMMITTERS="$(cat committers)"
	#LAST="$(cat last)"
	#FIRST="$(cat first)"
	#NDAYS=$(( \( $LAST - $FIRST \) / 60 / 60  / 24))

	# Place committers left/right according to the number of their commits
	sgsh-wrap awk '{print $2}' |
	sort |
	sgsh-wrap uniq -c |
	sort -n |
	sgsh-wrap awk 'BEGIN {l = 0; r = '"'"'$(cat committers)'"'"';}
			{print NR % 2 ? l++ : --r, $2}' |
	sort -k2 &

	sort -k2 &
}} |
# Join committer positions with commit time stamps
join -j 2 - -
