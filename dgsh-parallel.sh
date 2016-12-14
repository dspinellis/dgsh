#!/bin/sh
#
# Shard, compute, collect using dgsh
#

usage()
{
   echo 'Usage: shcoco -n n|-f file|-l list shard compute collect'
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

shard="$1"
shift
compute="$1"
shift
collect="$1"
shift

# Ensure exactly three commands are specified
if [ ! "$shard" -o ! "$compute" -o ! "$collect" -o "$1" ] ; then
  usage
fi

# Ensure exactly one sharding target is specified
if [ ! "$nspec" ] || expr match $nspec .. >/dev/null ; then
  usage
fi

SCRIPT=/tmp/shcoco-$$

cat >$SCRIPT <<EOF
#!/usr/bin/env dgsh
#
# Automatically generated file
#

# Shard
$shard |

# Compute
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
  echo "$compute &" | sed "s/{}/$node/"
done >>$SCRIPT

cat >>$SCRIPT <<EOF
}} |
# Collect
$collect
EOF

dgsh $SCRIPT
rm $SCRIPT
