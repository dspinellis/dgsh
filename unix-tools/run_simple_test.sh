#!/usr/bin/env bash

# Color
GR="\033[0;32m"	# Green
R="\033[0;31m"	# Red
B="\033[0;34m"	# Blue
EC="\033[0m"	# End color
S=${GR}successful${EC}
F=${R}failed${EC}

DGSH='bash/bash --dgsh'

PSDIR=$1
FILENAME=$2
SCRIPT=$3

PATH="$(pwd)/../build/libexec/dgsh:$(pwd)/../build/bin:$PATH" \
DGSH_DEBUG_LEVEL=4 $DGSH -c "$SCRIPT > $PSDIR/$FILENAME.outb" \
2>$PSDIR/$FILENAME.errb \
&& diff $PSDIR/$FILENAME.outb $PSDIR/$FILENAME.success \
&& printf "$FILENAME $S\n" \
|| (printf "$FILENAME $F\n" \
&& exit 1)

