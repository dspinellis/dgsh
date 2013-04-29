#!/usr/local/bin/sgsh
#
# SYNOPSIS Compare gzip vs bzip2 performance
# DESCRIPTION
# Compress the files in the specified directory, and report
# the ten best performing files for each compression program
# Demonstrates multiple scatter gather blocks and piping between them,
# piping into complex commands
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

# Read original-size name gzip-size bzip-size
# Report name, gzip-ratio, bzip-ratio
compression_ratio()
{
	awk '{
		gz = ($3 - $1) / $1
		bz = ($4 - $1) / $1
		print $2, gz, bz
	}'
}

find "$1" -type f |
head - 10 |
scatter |{
	-| xargs -n 1 wc -c |>/stream/wc
	-| while read f ; do gzip -c $f | wc -c ; done |>/stream/gzip
	-| while read f ; do bzip2 -c $f | wc -c ; done |>/stream/bzip2
|} gather |{
	paste /stream/wc /stream/gzip /stream/bzip2
|} |
scatter |{
	-| compression_ratio | sort -k2n | head -10 |>/stream/tgzip
	-| compression_ratio | sort -k3n | head -10 |>/stream/tbzip2
|} gather |{
	echo "Top gzip"
	cat /stream/tgzip
	echo "Top bzip"
	cat /stream/tbzip2
|}
