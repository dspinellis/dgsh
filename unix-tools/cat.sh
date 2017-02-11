#!/usr/bin/env bash
#
# Implementation of POSIX tee through dgsh-tee
#

usage()
{
  echo 'Usage: cat [-u] [file ...]' 1>&2
  exit 2
}

while getopts 'u' o; do
  case "$o" in
    u)
      ;;
    *)
      usage
      ;;
  esac
done

shift $((OPTIND-1))

declare -a opts

# Process file arguments
for i; do
    opts+=('-i' "$i")
    shift
done

exec dgsh-tee "${opts[@]}"
