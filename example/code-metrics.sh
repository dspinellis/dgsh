#!/usr/local/bin/sgsh
#
# C code metrics
# Demonstrates nesting, commands without input
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

scatter |{

	# C and header code
	.| find "$@" \( -name \*.c -or -name \*.h \) -type f -print0 |{

		# Average file name length
		# Convert to newline separation for counting
		-| tr \\0 \\n |
		# Remove path
		sed 's|^.*/||' |
		# Maintain average
		awk '{s += length($1); n++} END {print s / n}' |store:FNAMELEN

		-| xargs -0 cat |{

			# Remove strings and comments
			-| sed 's/#/@/g;s/\\[\\"'\'']/@/g;s/"[^"]*"/""/g;'"s/'[^']*'/''/g" |
			cpp -P 2>/dev/null |{

				# Structure definitions
				-|  egrep -c 'struct[ 	]*{|struct[ 	]*[a-zA-Z_][a-zA-Z0-9_]*[       ]*{' |store:NSTRUCT
				#}}

				# Type definitions
				-| grep -cw typedef |store:NTYPEDEF

				# Use of void
				-| grep -cw void |store:NVOID

				# Use of gets
				-| grep -cw gets |store:NGETS

				# Average identifier length
				-| tr -cs 'A-Za-z0-9_' '\n' |
				sort -u |
				awk '/^[A-Za-z]/ { len += length($1); n++ } END {print len / n}' |store:IDLEN
			|}

			# Lines and characters
			-| wc -lc | awk '{OFS=":"; print $1, $2}' |store:CHLINESCHAR

			# Non-comment characters (rounded thousands)
			# -traditional avoids expansion of tabs
			# We round it to avoid failing due to minor
			# differences between preprocessors in regression
			# testing
			-| sed 's/#/@/g' |
			cpp -traditional -P 2>/dev/null |
			wc -c |
			awk '{OFMT = "%.0f"; print $1/1000}' |store:NCCHAR

			# Number of comments
			-| egrep -c '/\*|//' |store:NCOMMENT

			# Occurences of the word Copyright
			-| grep -ci copyright |store:NCOPYRIGHT
		|}
	|}

	# C files
	.| find "$@" -name \*.c -type f -print0 |{

		# Convert to newline separation for counting
		-| tr \\0 \\n |{

			# Number of C files
			-| wc -l |store:NCFILE

			# Number of directories containing C files
			-| sed 's,/[^/]*$,,;s,^.*/,,' | sort -u | wc -l |store:NCDIR
		|}

		# C code
		-| xargs -0 cat |{

			# Lines and characters
			-| wc -lc | awk '{OFS=":"; print $1, $2}' |store:CLINESCHAR

			# C code without comments and strings
			-| sed 's/#/@/g;s/\\[\\"'\'']/@/g;s/"[^"]*"/""/g;'"s/'[^']*'/''/g" |
			cpp -P 2>/dev/null |{

				# Number of functions
				-| grep -c '^{' |store:NFUNCTION
				# } (match preceding open)

				# Number of gotos
				-| grep -cw goto |store:NGOTO

				# Occurrences of the register keyword
				-| grep -cw register |store:NREGISTER

				# Number of macro definitions
				-| grep -c '@[ 	]*define[ 	][ 	]*[a-zA-Z_][a-zA-Z0-9_]*(' |store:NMACRO

				# Number of include directives
				-| grep -c '@[ 	]*include' |store:NINCLUDE

				# Number of constants
				-| grep -w  -o '[0-9][x0-9][0-9a-f]*' | wc -l |store:NCONST

			|}
		|}
	|}

	# Header files
	.| find "$@" -name \*.h -type f | wc -l |store:NHFILE

# Gather and print the results
|} gather |{
cat <<EOF
FNAMELEN: `store:FNAMELEN`
NSTRUCT: `store:NSTRUCT`
NTYPEDEF: `store:NTYPEDEF`
IDLEN: `store:IDLEN`
CHLINESCHAR: `store:CHLINESCHAR`
NCCHAR: `store:NCCHAR`
NCOMMENT: `store:NCOMMENT`
NCOPYRIGHT: `store:NCOPYRIGHT`
NCFILE: `store:NCFILE`
NCDIR: `store:NCDIR`
CLINESCHAR: `store:CLINESCHAR`
NFUNCTION: `store:NFUNCTION`
NGOTO: `store:NGOTO`
NREGISTER: `store:NREGISTER`
NMACRO: `store:NMACRO`
NINCLUDE: `store:NINCLUDE`
NCONST: `store:NCONST`
NVOID: `store:NVOID`
NHFILE: `store:NHFILE`
NGETS: `store:NGETS`
EOF
|}
