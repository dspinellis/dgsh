#!/usr/bin/env bash
#!dgsh
#
# Dgsh-compatible wrapping of the GNU diff3 command
# Depending on the arguments, the command can accept 0-3 inputs
#


# For the parsing of long options see
# https://stackoverflow.com/questions/402377/using-getopts-in-bash-shell-script-to-get-long-and-short-command-line-options/7680682#7680682
optspec=":AeE3xXimaTL:v-:"
while getopts "$optspec" optchar; do
  case "${optchar}" in
    -)
      case "${OPTARG}" in
	# Long options with mandatory arguments. In these the argument
	# can appear separated from the option name, and must be removed.
	diff-program|label)
	  OPTIND=$(($OPTIND + 1))
	  ;;
      esac
      ;;
  esac
done

# Examine the number of remaining (non-option) arguments
case $(($# - $OPTIND + 1)) in
  0)
    # Exactly three inputs are required
    exec dgsh-wrap /usr/bin/diff3 "$@" '<|' '<|' '<|'
    ;;
  1)
    if [[ "${!OPTIND}" = '-' ]] ; then
      # Two inputs (excluding stdin) are required
      exec dgsh-wrap -I /usr/bin/diff3 "$@" '<|' '<|'
    else
      # Two inputs (including stdin) are required
      exec dgsh-wrap /usr/bin/diff3 "$@" '<|' '<|'
    fi
    ;;
  2)
    ARG2=$((OPTIND + 1))
    if [[ "${!OPTIND}" = '-' ]] || [[ "${!ARG2}" = '-' ]] ; then
      # One (non-stdin) input is required
      exec dgsh-wrap -I /usr/bin/diff3 "$@" '<|'
    else
      # One (stdin) input is required
      exec dgsh-wrap /usr/bin/diff3 "$@" -
    fi
    ;;
  *)
    ARG2=$((OPTIND + 1))
    ARG3=$((OPTIND + 2))
    if [[ "${!OPTIND}" = '-' ]] || [[ "${!ARG2}" = '-' ]] || [[ "${!ARG3}" = '-' ]] ; then
      # One (stdin) input is required
      exec dgsh-wrap /usr/bin/diff3 "$@"
    else
      # No input is required
      exec dgsh-wrap -i 0 /usr/bin/diff3 "$@"
    fi
    ;;
esac
