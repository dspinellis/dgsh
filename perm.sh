#!/bin/sh
#
# Permute inputs to outputs by invoking dgsh-tee -p
#

usage()
{
   echo 'Usage: perm n1,n2[, ...]'
   exit 2
}

# Get first argument. Its syntax will be validated by dgsh-tee
p="$1"
shift

# Ensure no more arguments are provided
for i; do
    usage
done

exec @libexecdir@/dgsh/dgsh-tee -p "$p"
