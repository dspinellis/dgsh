#!/bin/sh

PSDIR=$1
FSCRIPT=$2
BSCRIPT=$(basename $2 .sh)
INPUT_TYPE=$3
INPUT1=$4
INPUT2=$5
INPUT3=$6

if [ -d "bash" ]; then
	DGSH='bash/bash --dgsh'
else
	DGSH='../bash/bash --dgsh'
fi

GR="\033[0;32m"	# Green
R="\033[0;31m"	# Red
B="\033[0;34m"	# Blue
EC="\033[0m"	# End color
S=${GR}successful${EC}
F=${R}failed${EC}

# Skip test if required custom commands are missing
(
	iscommand=0
	for arg in "$@"; do
		if [ "$arg" = "--" ]; then
			iscommand=1
			continue
		fi
		if [ $iscommand -eq 0 ]; then
			continue
		fi
		if ! which $arg >/dev/null; then
			exit 1
		fi
	done
	exit 0
)

if [ $? -ne 0 ]; then
	echo "Skip test $BSCRIPT.sh"
fi

PATH="$(pwd)/../build/libexec/dgsh:$(pwd)/../build/bin:$PATH"

if [ "$INPUT_TYPE" = pipe ]; then
  if $DGSH $FSCRIPT <$INPUT1 >$PSDIR/$BSCRIPT.outb 2>$PSDIR/$BSCRIPT.err ; then
    printf "$BSCRIPT.sh $S\n"
  else
    printf "$BSCRIPT.sh $F\n"
    exit 1
  fi
else
  if $DGSH $FSCRIPT $INPUT1 $INPUT2 $INPUT3 >$PSDIR/$BSCRIPT.outb \
	  2>$PSDIR/$BSCRIPT.err ; then
    printf "$BSCRIPT.sh $S\n"
  else
    printf "$BSCRIPT.sh $F\n"
    exit 1
  fi
fi
