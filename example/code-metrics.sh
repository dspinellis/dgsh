#!/usr/local/bin/sgsh
#
# C code metrics
#

scatter |{

	# C and header code
	find "$@" \( -name \*.c -or -name \*.h \) -type f -print0 |{

		# Average file name length
		# Convert to newline separation for counting
		-| tr \\0 \\n |
		# Remove path
		sed 's|^.*/||' |
		# Maintain average
		awk '{s += length($1); n++} END {print s / n}' |=FNAMELEN

		-| xargs -0 cat |{

			# Remove strings and comments
			-| sed 's/#/@/g;s/\\[\\"'\'']/@/g;s/"[^"]*"/""/g;'"s/'[^']*'/''/g" |
			cpp -P 2>/dev/null |{

				# Structure definitions
				-|  egrep 'struct[ 	]*{|struct[ 	]*[a-zA-Z_][a-zA-Z0-9_]*[       ]*{' |
				wc -l |=NSTRUCT

				# Type definitions
				-| grep -w typedef | wc -l |=NTYPEDEF

				# Use of void
				-| grep -w void | wc -l |=NVOID

				# Use of gets
				-| grep -w gets | wc -l |=NGETS

				# Average identifier length
				-| tr -cs 'A-Za-z0-9_' '\n' |
				sort -u |
				awk '/^[A-Za-z]/ { len += length($1); n++ } END {print len / n}' |=IDLEN
			|}

			# Lines and characters
			-| wc -lc | awk '{OFS=":"; print $1, $2}' |=CHLINESCHAR

			# Non-comment characters (rounded thousands)
			# -traditional avoids expansion of tabs
			# We round it to avoid failing due to minor
			# differences between preprocessors in regression
			# testing
			-| sed 's/#/@/g' |
			cpp -traditional -P 2>/dev/null |
			wc -c |
			awk '{OFMT = "%.0f"; print $1/1000}' |=NCCHAR

			# Number of comments
			-| egrep '/\*|//' | wc -l |=NCOMMENT

			# Occurences of the word Copyright
			-| grep -i copyright | wc -l |=NCOPYRIGHT
		|}
	|}

	# C files
	find "$@" -name \*.c -type f -print0 |{

		# Convert to newline separation for counting
		-| tr \\0 \\n |{

			# Number of C files
			-| wc -l |=NCFILE

			# Number of directories containing C files
			-| sed 's,/[^/]*$,,;s,^.*/,,' | sort -u | wc -l |=NCDIR
		|}

		# C code
		-| xargs -0 cat |{

			# Lines and characters
			-| wc -lc | awk '{OFS=":"; print $1, $2}' |=CLINESCHAR

			# C code without comments and strings
			-| sed 's/#/@/g;s/\\[\\"'\'']/@/g;s/"[^"]*"/""/g;'"s/'[^']*'/''/g" |
			cpp -P 2>/dev/null |{

				# Number of functions
				-| grep '^{' | wc -l |=NFUNCTION
				# } (match preceding open)

				# Number of gotos
				-| grep -w goto | wc -l |=NGOTO

				# Occurrences of the register keyword
				-| grep -w register | wc -l |=NREGISTER

				# Number of macro definitions
				-| grep '@[ 	]*define[ 	][ 	]*[a-zA-Z_][a-zA-Z0-9_]*(' | wc -l |=NMACRO

				# Number of include directives
				-| grep '@[ 	]*include' | wc -l |=NINCLUDE

				# Number of constants
				-| grep -w  -o '[0-9][x0-9][0-9a-f]*' | wc -l |=NCONST

			|}
		|}
	|}

	# Header files
	find "$@" -name \*.h -type f | wc -l |=NHFILE

# Gather and print the results
|} gather |{
	echo "FNAMELEN: $FNAMELEN"
	echo "NSTRUCT: $NSTRUCT"
	echo "NTYPEDEF: $NTYPEDEF"
	echo "IDLEN: $IDLEN"
	echo "CHLINESCHAR: $CHLINESCHAR"
	echo "NCCHAR: $NCCHAR"
	echo "NCOMMENT: $NCOMMENT"
	echo "NCOPYRIGHT: $NCOPYRIGHT"
	echo "NCFILE: $NCFILE"
	echo "NCDIR: $NCDIR"
	echo "CLINESCHAR: $CLINESCHAR"
	echo "NFUNCTION: $NFUNCTION"
	echo "NGOTO: $NGOTO"
	echo "NREGISTER: $NREGISTER"
	echo "NMACRO: $NMACRO"
	echo "NINCLUDE: $NINCLUDE"
	echo "NCONST: $NCONST"
	echo "NVOID: $NVOID"
	echo "NHFILE: $NHFILE"
	echo "NGETS: $NGETS"
|}
