#!/bin/sh
#
# Install the commands wrapped by dgsh-wrap
#

DGPATH=$DESTDIR$PREFIX/libexec/dgsh
mkdir -p $DGPATH

# Remove comments and blank lines
sed 's/[ \t]*#.*//;/^$/d' wrapped-commands-posix wrapped-commands-tests |
while read mode name ; do
  # Continue if command is not available
  if ! path=$(which $name 2>/dev/null) ; then
    continue
  fi

  # Continue if command is custom-implemented or both dead and mute
  if [ $mode = c -o $mode = dm ] ; then
    continue
  fi

  # Iterate over the mode's characters creating $opt
  opt=''
  for m in $(echo "$mode" | sed 's/./& /g') ; do
    case $m in
      m)	# Mute
	opt="$opt -m"
	;;
      M)	# Mute unless - is specified (TODO)
	;;
      d)	# Deaf
	opt="$opt -d"
	;;
      s)	# Count stdin in channel assignments
	opt="$opt -s"
	;;
      D)	# Deaf unless - is specified or no arguments are provided (TODO)
	;;
      f)	# Filter
	;;
      *)
	echo "Unknown I/O mode character $m for $name" 1>&2
	exit 1
    esac
  done
  target=$DGPATH/$name
  echo "#!$DGPATH/dgsh-wrap$opt $path" >$target
  chmod 755 $target
done
