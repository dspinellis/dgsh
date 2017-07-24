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
#   - common arguments
#   - multiple input streams, e.g. -f -
#   - testing
#   - substitute grep

#command="$0"
command="grep" #Temporary

declare -A shortopts
declare -A longopts
sargs=-1
largs=-1

# segregate short opts from long opts, pattern, target
for arg in "$@"; do
	if [ `expr match "$arg" '-[a-z[A-Z]'` -eq 2 ] ; then
		echo "match shortopt: $arg"
		(( sargs++ ))
		shortopts[$sargs]="$arg"
		is_shortopt=1
	elif [ $is_shortopt -eq 1 ] && [ `expr match "$arg" '[a-z[A-Z]+'` -gt 1 ] ; then
		echo "optarg: $arg"
		shortopts[$sargs]="$arg"
		(( sargs++ ))
		is_shortopt=0
	else
		echo "long opt or target files: $arg"
		longopts[$largs]="$arg"
		(( largs++ ))
	fi
done
	
echo "command: $command"
echo "sargs: $sargs"
echo "largs: $largs"

# Process flags
while getopts 'df:l:n:' o; do
  case "$o" in
    d)
      DEBUG=1
      ;;
    n)
      n="$OPTARG"
      nspec=X$nspec
      ;;
    f)
      file="$OPTARG"
      nspec=X$nspec
      ;;
    -l)
      list=$(echo "$OPTARG" | sed 's/,/ /g')
      nspec=X$nspec
      ;;
    *)
      usage
      ;;
  esac
done

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

if [ $sargs -gt 1 ]; then
cat >$SCRIPT <<EOF
{{
EOF
fi

for arg in ${shortopts[*]} ; do
	echo "	$command $arg ${longopts[*]}"
done >>$SCRIPT

if [ $sargs -gt 1 ]; then
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
