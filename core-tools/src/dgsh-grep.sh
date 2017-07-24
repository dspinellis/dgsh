#!/usr/bin/env bash
#
# Create and execute a dgsh graph of grep commands
# that is equivalent to an incoming grep command
# with the difference that it exposes multiple input
# and output streams
#

# Remove dgsh from path, so that commands aren't wrapped here
# See http://stackoverflow.com/a/2108540/20520
# PATH => /bin:.../libexec/dgsh:/sbin
OPATH="$PATH"
WORK=:$PATH:
# WORK => :/bin:.../libexec/dgsh:/sbin:
REMOVE='[^:]*/libexec/dgsh'
WORK=${WORK/:$REMOVE:/:}
# WORK => :/bin:/sbin:
WORK=${WORK%:}
WORK=${WORK#:}
PATH=$WORK
# PATH => /bin:/sbin

# Remove DGSH_IN, OUT so that commands don't negotiate
test "$DGSH_IN" && ODGSH_IN="$DGSH_IN"
test "$DGSH_OUT" && ODGSH_OUT="$DGSH_OUT"
unset DGSH_IN
unset DGSH_OUT

usage()
{
	echo 'Usage: dgsh-grep [-d] [-c] [-f] [-j] [-l] [-L] [-o] [-v] [file]' 1>&2
	exit 2
}

# TODO:
#   - multiple input streams, e.g. -f -
#   - testing
#   - substitute grep

#command="$0"
command="grep" #Temporary

outopts=("-c" "-d" "-j" "-l" "-L" "-o" "-v")
declare -a streamopts
declare -a restopts

# segregate short opts that indicate IO channels from everything else
for arg in "$@"; do
	#echo "arg: $arg"
	if [ `expr match "$arg" '-[a-z[A-Z]'` -eq 2 ] ; then
		# echo "short opt: $arg"
		for pos in $(seq ${#arg[*]}) ; do
			char=${arg:$pos:1}
			# echo "char: $char"
			is_streamopt=0
			for out in ${outopts[*]}; do
				if [ "-$char" == "$out" ] ; then
					#echo "match streamopt: $arg"
					streamopts+=("$arg")
					is_streamopt=1
					break
				fi
			done
			if [ $is_streamopt -eq 0 ] ; then
				#echo "other short opt: $arg"
				restopts+=("$arg")
			fi
		done
	else
		#echo "other arg: $arg"
		restopts+=("$arg")
	fi
done
	
echo "command: $command"
echo "streamopts len: ${#streamopts[*]}"
echo "streamopts: ${streamopts[*]}"
echo "restopts len: ${#restopts[*]}"
echo "restopts: ${restopts[*]}"

# Process flags
while getopts 'cdf:jlLov' o ${streamopts[*]} ; do
	case "$o" in
		c)
			args+=("-$o")
			;;
		d)
			DEBUG=1
			;;
		f)
			args+=("-$o")
			args+=("$OPTARG")
			;;
		j)
			args+=("-$o")
			;;
		l)
			args+=("-$o")
			;;
		L)
			args+=("-$o")
			;;
		o)
			args+=("-$o")
			;;
		v)
			args+=("-$o")
			;;
		*)
			usage
			;;
	esac
done

echo "args len: ${#args[*]}"
echo "args: ${args[*]}"

# Ensure generated script is always removed
SCRIPT="${TMP:-/tmp}/dgsh-grep-$$"

if [ "$DEBUG" ] ; then
  echo "Script is $SCRIPT" 1>&2
else
  trap 'rm -rf "$SCRIPT"' 0
  trap 'exit 2' 1 2 15
fi

cat >$SCRIPT <<EOF
#!/usr/bin/env dgsh
#
# Automatically generated file from:
# $command $*
#

EOF

if [ ${#args[*]} -gt 1 ]; then
cat >$SCRIPT <<EOF
{{
EOF
fi

for arg in ${args[*]} ; do
	echo "	$command $arg ${restopts[*]}"
done >>$SCRIPT

if [ ${#args[*]} -gt 1 ]; then
cat >>$SCRIPT <<EOF
}}
EOF
fi

cat $SCRIPT
exit 0


# Restore dgsh settings
PATH="$OPATH"
# Remove DGSH_IN, OUT so that commands don't negotiate
test "$ODGSH_IN" && export DGSH_IN="$ODGSH_IN"
test "$ODGSH_OUT" && export DGSH_OUT="$ODGSH_OUT"

dgsh $SCRIPT
