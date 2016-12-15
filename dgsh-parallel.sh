#!/usr/bin/env bash
#
# Create and execute a semi-homongeneous dgsh parallel processing block
#

# Ensure generated script is always removed
SCRIPT="${TMP:-/tmp}/dgsh-parallel-$$"
trap 'rm -rf "$SCRIPT"' 0
trap 'exit 2' 1 2 15

# Remove dgsh from path, so that commands aren't wrapped here
# See http://stackoverflow.com/a/2108540/20520
# PATH => /bin:.../libexec/dgsh:/sbin
WORK=:$PATH:
# WORK => :/bin:.../libexec/dgsh:/sbin:
REMOVE='[^:]*/libexec/dgsh'
WORK=${WORK/:$REMOVE:/:}
# WORK => :/bin:/sbin:
WORK=${WORK%:}
WORK=${WORK#:}
PATH=$WORK
# PATH => /bin:/sbin

cat >$SCRIPT <<EOF
#!/usr/bin/env dgsh
#
# Automatically generated file from:
# $0 $*
#

{{
EOF

usage()
{
   echo 'Usage: dgsh-processing -n n|-f file|-l list command ...'
   exit 2
}

# Process flags
args=$(getopt f:l:n: "$@")
if [ $? -ne 0 ]; then
  usage
fi

for i in $args; do
   case "$1" in
   -n)
     n="$2"
     nspec=X$nspec
     shift
     shift
     ;;
   -f)
     file="$2"
     nspec=X$nspec
     shift
     shift
     ;;
   -l)
     list=$(echo "$2" | sed 's/,/ /g')
     nspec=X$nspec
     shift
     shift
     ;;
   --)
	   shift; break
	   ;;
   esac
done

# Ensure commands is specified
if [ ! "$1" ] ; then
  usage
fi

# Ensure exactly one sharding target is specified
if [ ! "$nspec" ] || expr match $nspec .. >/dev/null ; then
  usage
fi


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
  echo "$1 &" | sed "s/{}/$node/"
done >>$SCRIPT

cat >>$SCRIPT <<EOF
}}
EOF

exec dgsh --dgsh-negotiate $SCRIPT
