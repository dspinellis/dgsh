#!/bin/sh
#
# Install the commands wrapped by dgsh-wrap
#

mkdir -p $PREFIX/libexec/dgsh

# Remove comments and blank lines
sed 's/[ \t]*#.*//;/^$/d' wrapped-commands |
while read mode name ; do
  if ! path=$(which $name) ; then
    continue
  fi
  case $mode in
    m) opt='-m '
      ;;
    d) opt='-d '
      ;;
    f) opt=''
      ;;
    *)
      echo "Unknown I/O mode $mode" 1>&2
      exit 1
  esac
  target=$PREFIX/libexec/dgsh/$name
  echo "#!$PREFIX/libexec/dgsh/dgsh-wrap $opt$path" >$target
  chmod 755 $target
done
