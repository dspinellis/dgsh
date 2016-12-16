#!/bin/sh

PSDIR=$1
FSCRIPT=$2
BSCRIPT=$(basename $2 .sh)
INPUT_TYPE=$3
INPUT1=$4
INPUT2=$5
INPUT3=$6

GR="\033[0;32m"	# Green
R="\033[0;31m"	# Red
B="\033[0;34m"	# Blue
EC="\033[0m"	# End color
S=${GR}successful${EC}
F=${R}failed${EC}

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

if [ $? -eq 0 ]; then
	if [ "$INPUT_TYPE" = "pipe" ]; then
		cat $INPUT1 | dgsh $FSCRIPT \
			>$PSDIR/$BSCRIPT.outb \
			2>$PSDIR/$BSCRIPT.err \
		&& printf "$BSCRIPT.sh $S\n" \
		|| (printf "$BSCRIPT.sh $F\n" \
		&& exit 1)
	else
		dgsh $FSCRIPT $INPUT1 $INPUT2 $INPUT3 \
			>$PSDIR/$BSCRIPT.outb \
			2>$PSDIR/$BSCRIPT.err \
		&& printf "$BSCRIPT.sh $S\n" \
		|| (printf "$BSCRIPT.sh $F\n" \
		&& exit 1)
	fi
else
	echo "Skip test $BSCRIPT.sh"
fi
