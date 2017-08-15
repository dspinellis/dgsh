#!/bin/sh
#
# Regression testing of the provided examples
#

TOP=$(cd ../.. ; pwd)
DGSH="$TOP/build/bin/dgsh"
PATH="$TOP/build/bin:$PATH"
export DGSHPATH="$TOP/build/libexec/dgsh"

# Ensure that the generated test file matches the reference one
# File names are by conventions dgsh-wrap/$case.{ok,test}
ensure_same()
{
  local case=$1
  echo -n "$case "
  if diff dgsh-wrap/$case.ok dgsh-wrap/$case.test
  then
    echo OK
  else
    echo "$case: Files differ: dgsh-wrap/$case.ok dgsh-wrap/$case.test" 1>&2
    exit 1
  fi
}

# Include fallback commands in our executable path
export PATH="$PATH:bin"

# Test that echo is wrapped as deaf
$DGSH -c 'dgsh-enumerate 1 | {{ dgsh-wrap -d echo hi ; dgsh-wrap dd 2>/dev/null ; }} | cat' >dgsh-wrap/echo-deaf.test
ensure_same echo-deaf

# Test that echo is wrapped as deaf when wrapped as script with supplied exec
echo "#!$TOP/build/libexec/dgsh/dgsh-wrap -S  -d `which echo`" >dgsh-wrap/echo-S
chmod +x dgsh-wrap/echo-S
$DGSH -c 'dgsh-enumerate 1 | {{ dgsh-wrap/echo-S hi ; dgsh-wrap dd 2>/dev/null ; }} | cat' >dgsh-wrap/echo-S.test
ensure_same echo-S

# Test that echo is wrapped as deaf when wrapped as script with implied exec
echo "#!$TOP/build/libexec/dgsh/dgsh-wrap -s  -d" >dgsh-wrap/echo
chmod +x dgsh-wrap/echo
$DGSH -c 'dgsh-enumerate 1 | {{ dgsh-wrap/echo hi ; dgsh-wrap dd 2>/dev/null ; }} | cat' >dgsh-wrap/echo-s.test
ensure_same echo-s

# Verify the fdescfs functionality (required by dgsh-wrap <| and >|
# arguments) is available
if [ $(ls /dev/fd | wc -l) -ne 4 ] ; then
  cat <<\EOF 1>&2
The full functionality of dgsh-wrap requires full /dev/fd functionality;
i.e.  that *all* file descriptors of a process are available under /dev/fd.
It appears that on this system only the first three file descriptors
are available under /dev/fd.  Consequently, dgsh-wrap will not be able
support <| and >| arguments and dependent programs (e.g. dgsh-parallel)
will fail.

In FreeBSD full /dev/fd functionality is provided by fdescfs; the
functionality that FreeBSD devfs provides by default only includes
the first three file descriptors under /dev/fd.  In FreeBSD systems
consider mounting fdescfs(5) on /dev/fd to avoid this problem.
EOF
  exit 1
else
  # Test stand-alone path substitution (with stdin)
  $DGSH -c 'dgsh-enumerate 2 | dgsh-wrap paste "<|" "<|" ' >dgsh-wrap/paste2.test
  ensure_same paste2

  # Test stand-alone path substitution (without stdin)
  $DGSH -c 'dgsh-enumerate 2 | dgsh-wrap -I /usr/bin/paste - "<|" ' >dgsh-wrap/paste1.test
  ensure_same paste1

  # Test substitution of embedded arguments
  $DGSH -c 'dgsh-enumerate 1 | {{ dgsh-wrap -e dd "if=<|" "of=>|" 2>/dev/null ; }} | cat' >dgsh-wrap/dd-args.test
  ensure_same dd-args
fi

exit 0
