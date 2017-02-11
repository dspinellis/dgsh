#!/usr/bin/env bash
#
# Implementation of POSIX tee through dgsh-tee
#

usage()
{
  echo 'Usage: tee [-ai] [file ...]' 1>&2
  exit 2
}

declare -a opts

# Process flags
while getopts 'ai' o; do
  case "$o" in
    a)
      opts=(-a)
      ;;
    i)
      ;;
    *)
      usage
      ;;
  esac
done

shift $((OPTIND-1))

# Process file arguments
for i; do
    opts+=('-o' "$i")
    shift
done

exec dgsh-tee "${opts[@]}"
