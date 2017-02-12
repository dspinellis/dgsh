#!/bin/sh
#
# Run any dgsh script from the built (rather than the installed)
# version of dgsh.
# The first argument must be the directory containing the dgsh sources.
#

if [ -z "$1" ] ; then
  echo "Usage: $0 dgsh-root-path [dgsh arguments]" 1>&2
  exit 1
fi

TOP="$1"
shift
DGSH="$TOP/build/bin/dgsh"
PATH="$TOP/build/bin:$PATH"
export DGSHPATH="$TOP/build/libexec/dgsh"

dgsh "$@"
