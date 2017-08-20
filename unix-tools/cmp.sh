#!/usr/bin/env bash
#!dgsh
#
# Dgsh-compatible wrapping of the GNU cmp command
# Depending on the arguments, the command can accept 0-2 inputs
#


oopt=''

# For the parsing of long options see
# https://stackoverflow.com/questions/402377/using-getopts-in-bash-shell-script-to-get-long-and-short-command-line-options/7680682#7680682
optspec=":bi:ln:sv-:"
while getopts "$optspec" optchar; do
  case "${optchar}" in
    -)
      case "${OPTARG}" in
        quiet)
          oopt='-o 0'
          ;;
        silent)
          oopt='-o 0'
          ;;
	# Long options with mandatory arguments. In these the argument
	# can appear separated from the option name, and must be removed.
	ignore-initial|bytes)
	  OPTIND=$(($OPTIND + 1))
	  ;;
      esac
      ;;
    s)
      oopt='-o 0'
      ;;
  esac
done

# Examine the number of remaining (non-option) arguments
case $(($# - $OPTIND + 1)) in
  0)
    # Exactly two inputs are required
    exec dgsh-wrap $oopt /usr/bin/cmp "$@" '<|' '<|'
    ;;
  1)
    if [[ "${!OPTIND}" == '-' ]] ; then
      # One (non-stdin) input is required
      exec dgsh-wrap -I $oopt /usr/bin/cmp "$@" '<|'
    else
      # One (stdin) input is required
      exec dgsh-wrap $oopt /usr/bin/cmp "$@" -
    fi
    ;;
  *)
    ARG2=$((OPTIND + 1))
    if [[ "${!OPTIND}" == '-' ]] || [[ "${!ARG2}" == '-' ]] ; then
      # One (stdin) input is required
      exec dgsh-wrap /usr/bin/cmp $oopt "$@"
    else
      # No input is required
      exec dgsh-wrap -i 0 /usr/bin/cmp $oopt "$@"
    fi
    ;;
esac
