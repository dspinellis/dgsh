#!/bin/sh
#
# Portable time function
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

# Run the command with the specified description
# logging the output and time to {out,err,time} directories
timerun()
{
	local -r DESC="$1"
	shift
	case `uname` in
	FreeBSD|Darwin)
		/usr/bin/time -l sh -c "$@ >out/$DESC 2>err/$DESC" 2>time/$DESC
		;;
	Linux|CYGWIN*)
		/usr/bin/time -v -o time/$DESC "$@" >out/$DESC 2>err/$DESC
		;;
	esac
}

# Download the specified URL, if needed, to a file of its last component
download()
{
	local -r URL="$1"
	FILENAME="`basename $URL`"
	if ! [ -f "$FILENAME" ]
	then
		wget $URL 2>/dev/null ||
		curl $URL >$FILENAME
	fi
}

# Output the specified URL to standard output
download_stdout()
{
	local -r URL="$1"
	curl $URL 2>/dev/null ||
	wget -O - $URL
}
