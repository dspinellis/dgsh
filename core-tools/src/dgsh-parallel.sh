#!/usr/bin/env bash
#!dgsh
#
# Create and execute a semi-homongeneous dgsh parallel processing block
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
  echo 'Usage: dgsh-parallel [-d] -n n|-f file|-l list command ...' 1>&2
  exit 2
}

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

shift $((OPTIND-1))


# Ensure commands is specified
if [ ! "$1" ] ; then
  usage
fi

# Ensure exactly one sharding target is specified
if [ ! "$nspec" ] || expr $nspec : .. >/dev/null ; then
  usage
fi

# Ensure generated script is always removed
SCRIPT="${TMP:-/tmp}/dgsh-parallel-$$"

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
# $0 $*
#

{{
EOF


# Generate list of nodes
if [ "$n" ] ; then
  for i in $(seq "$n") ; do
    echo $i
  done
elif [ "$list" ] ; then
  for i in $list ; do
    echo "$i"
  done
elif [ "$file" ] ; then
  cat "$file"
else
  echo Internal error 1>&2
  exit 2
fi |
# Escape sed(1) special characters
sed 's/[&/\\]/\\&/g' |
# Replace {} with the name of each node
while IFS='' read -r node ; do
  echo "  $@" | sed "s/{}/$node/"
done >>$SCRIPT

cat >>$SCRIPT <<EOF
}}
EOF

# Restore dgsh settings
PATH="$OPATH"
# Remove DGSH_IN, OUT so that commands don't negotiate
test "$ODGSH_IN" && export DGSH_IN="$ODGSH_IN"
test "$ODGSH_OUT" && export DGSH_OUT="$ODGSH_OUT"

dgsh $SCRIPT
