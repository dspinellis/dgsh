#!/usr/local/bin/sgsh
#
# Windows-like DIR command for the current directory
# Nothing that couldn't be done with ls -l | awk
# Demonstrates combined use of variable assignment and file redirection
# Tests auto-export of assigned variables to subshell
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
	-| awk '!/^total/ {print $6, $7, $8, $1, sprintf("%8d", $5), $9}' |>/sgsh/files

	# Count number of files
	-| wc -l |=NFILES

	# Count number of directories
	-| grep '^d' | wc -l |= NDIRS

	# Tally number of bytes
	-| awk '{s += $5} END {print s}' |=NBYTES
|} gather |{
	cat /sgsh/files
	echo "               $NFILES File(s) $NBYTES bytes"
	echo "               $NDIRS Dir(s) $FREE bytes free"
|}
