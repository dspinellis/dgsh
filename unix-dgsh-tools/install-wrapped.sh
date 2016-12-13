#!/bin/sh
#
# Install the commands wrapped by dgsh-wrap
#

DGPATH=$PREFIX/libexec/dgsh
mkdir -p $DGPATH

# Remove comments and blank lines
sed 's/[ \t]*#.*//;/^$/d' wrapped-commands |
while read mode name ; do
  if ! path=$(which $name) ; then
    continue
  fi
  case $mode in
    m)	# Mute
      opt=' -m'
      ;;
    d)	# Deaf
      opt=' -d'
      ;;
    f)	# Filter
      opt=''
      ;;
    c)	# Custom implementation
      continue
      ;;
    *)
      echo "Unknown I/O mode $mode" 1>&2
      exit 1
  esac
  target=$DGPATH/$name
  echo "#!$DGPATH/dgsh-wrap$opt $path" >$target
  chmod 755 $target
done
