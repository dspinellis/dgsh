#!/bin/sh
#
# Test the dgsh-compatibility checking

echo 'Testing dgsh compatibility checker'

check()
{
  local title="$1"
  local prog="$2"
  local is_dgsh="$3"

  echo -n "$title: "

  case $is_dgsh in
    F)
      if ./dgsh-compat "$prog" ; then
	echo FAIL
	exit 1
      else
	echo PASS
      fi
      ;;
    T)
      if ./dgsh-compat "$prog" ; then
	echo PASS
      else
	echo FAIL
	exit 1
      fi
      ;;
  esac
}


check 'Non-dgsh binary file' /bin/ls F

check 'Dgsh binary file' ../build/libexec/dgsh/dgsh-tee T

check 'Dgsh wrap script' ../build/libexec/dgsh/date T

check 'Dgsh magic script dgsh-parallel' ../build/libexec/dgsh/dgsh-parallel T

check 'Dgsh magic script tee' ../build/libexec/dgsh/tee T

check 'Dgsh magic script cat' ../build/libexec/dgsh/cat T

check 'Non-dgsh script' ./test-compat.sh F

exit 0
