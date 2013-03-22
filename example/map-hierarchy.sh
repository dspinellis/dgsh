#!/usr/local/bin/sgsh
#
# SYNOPSIS Hierarchy map
# DESCRIPTION
# Given two directory hierarchies A and B passed as input arguments
# (where these represent a project at different parts of its lifetime)
# copy the files of hierarchy A to a new directory "new" corresponding
# to the structure of directories in B.
# Demonstrates the use of <em>join</em> to gather results from streams
# in the <em>scatter</em> part,
# and the use of <em>cat</em> to order asynchronous results in the gather part.
#
#  Copyright 2012-2013 Diomidis Spinellis
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

export LC_ALL=C

line_signatures()
{
	find $1 -type f -name '*.[chly]' -print |
	# Split path name into directory and file
	sed 's|\(.*\)/\([^/]*\)|\1 \2|' |
	while read dir file
	do
		# Print "directory filename content" of lines with
		# at least one alphabetic character
		# The fields are separated by ^A and ^B
		sed -n "/[a-z]/s|^|$dir:$file;|p" "$dir/$file"
	done |
	sort -t: -k 2
}

scatter |{
	# Generate the signatures for the two hierarchies
	.| line_signatures $1 |>/stream/a
	.| line_signatures $2 |>/stream/b

	# Join signatures on file name and content
	.| join -t: -1 2 -2 2 /stream/a /stream/b |
	# Print filename dir1 dir2
	sed 's/;/:/g' |
	awk -F: 'BEGIN{OFS=" "}{print $1, $3, $4}' |
	# Unique occurrences
	sort -u |{
		# Commands to copy
		-| awk '{print "cp " $2 "/" $1 " new/" $3 "/" $1}' |>/stream/cp
		-| awk '{print "mkdir -p new/" $3}' | sort -u |>/stream/mkdir
	|}
|} gather |{
	# Order: first make directories, then copy files
	cat /stream/mkdir /stream/cp |
	sh
|}
