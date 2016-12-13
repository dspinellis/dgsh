#!/bin/sh
#
# Permute inputs to outputs by invoking dgsh-tee -p
#

usage()
{
   echo 'Usage: perm n1,n2[, ...]'
   exit 2
}

# Validate first argument syntax
expr match "$1" '[0-9][0-9,]*[0-9]$' >/dev/null || usage
p="$1"
shift

# Ensure no more arguments are provided
for i; do
    usage
done

exec @libexecdir@/dgsh/dgsh-tee -p "$p"
