#!/usr/local/bin/sgsh
#
# SYNOPSIS Directory listing
# DESCRIPTION
# Windows-like DIR command for the current directory.
# Nothing that couldn't be done with <code>ls -l | awk</code>.
# Demonstrates combined use of stores and streams.
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

FREE=`df -h . | awk '!/Use%/{print $4}'`

ls -n |
scatter |{
	# Reorder fields in DIR-like way
	-| awk '!/^total/ {print $6, $7, $8, $1, sprintf("%8d", $5), $9}' |>/stream/files

	# Count number of files
	-| wc -l |store:NFILES

	# Count number of directories
	-| grep -c '^d' |store:NDIRS

	# Tally number of bytes
	-| awk '{s += $5} END {print s}' |store:NBYTES
|} gather |{
	cat /stream/files
	echo "               `store:NFILES` File(s) `store:NBYTES` bytes"
	echo "               `store:NDIRS` Dir(s) $FREE bytes free"
|}
