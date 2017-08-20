#!/usr/bin/env bash
#!dgsh
#
# Dgsh-compatible wrapping of the GNU diff command
# Depending on the arguments, the command can accept 0-N inputs
#


# For the parsing of long options see
# https://stackoverflow.com/questions/402377/using-getopts-in-bash-shell-script-to-get-long-and-short-command-line-options/7680682#7680682
optspec=":qscC:uU:enyW:pF:tTlrNx:X:S:iEZbwBIaD:dv-:"
while getopts "$optspec" optchar; do
  case "${optchar}" in
    -)
      case "${OPTARG}" in
        from-file)
          from_file="${!OPTIND}"
	  OPTIND=$(($OPTIND + 1))
          ;;
        from-file=*)
	  from_file=${OPTARG#*=}
	  ;;
        to-file)
          to_file="${!OPTIND}"
	  OPTIND=$(($OPTIND + 1))
          ;;
        to-file=*)
	  to_file=${OPTARG#*=}
	  ;;
      esac
      ;;
  esac
done

# Examine the number of remaining (non-option) arguments
case $(($# - $OPTIND + 1)) in
  0)
    if [[ -n "$from_file" ]] || [[ -n "$to_file" ]] ; then
      # An arbitrary number of inputs can be handled
      exec dgsh-wrap -i a /usr/bin/diff "$@"
    else
      # Exactly two inputs are required
      exec dgsh-wrap /usr/bin/diff "$@" '<|' '<|'
    fi
    ;;
  1)
    if [[ -n "$from_file" ]] || [[ -n "$to_file" ]] ; then
      # No input is required
      exec dgsh-wrap -i 0 /usr/bin/diff "$@"
    elif [[ "${!OPTIND}" == '-' ]] ; then
      # One (non-stdin) input is required
      exec dgsh-wrap -I /usr/bin/diff "$@" '<|'
    else
      # One (stdin) input is required
      exec dgsh-wrap /usr/bin/diff "$@" -
    fi
    ;;
  *)
    ARG2=$((OPTIND + 1))
    if [[ "${!OPTIND}" == '-' ]] || [[ "${!ARG2}" == '-' ]] ; then
      # One (stdin) input is required
      exec dgsh-wrap /usr/bin/diff "$@"
    else
      # No input is required
      exec dgsh-wrap -i 0 /usr/bin/diff "$@"
    fi
    ;;
esac
