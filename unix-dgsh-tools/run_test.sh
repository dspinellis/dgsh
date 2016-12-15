#!/bin/sh

PSDIR=$1
SCRIPT=$(basename $2)
COMMANDS=$3
INPUT_TYPE=$4
INPUT1=$5
INPUT2=$6
INPUT3=$7

GR="\033[0;32m"	# Green
R="\033[0;31m"	# Red
B="\033[0;34m"	# Blue
EC="\033[0m"	# End color
S=${GR}successful${EC}
F=${R}failed${EC}

(
	if [ $COMMANDS -gt 0 ]; then
		i=0
		for arg in "$@"; do
			if [ $i -lt $COMMANDS ]; then
				((i++))
				continue
			fi
			if [ ! $(type $arg) ]; then
				exit 1
			fi
		done
		exit 0
	fi
)

if [ $? -eq 0 ]; then
	if [ "$INPUT_TYPE" = "pipe" ]; then
		cat $INPUT | dgsh $SCRIPT.sh \
			>$PSDIR/$SCRIPT.outb \
			2>$PSDIR/$SCRIPT.err \
		&& printf "$SCRIPT $S\n" \
		|| (printf "$SCRIPT $F\n" \
		&& exit 1)
	else
		dgsh $SCRIPT.sh $INPUT1 $INPUT2 $INPUT3 \
			>$PSDIR/$SCRIPT.outb \
			2>$PSDIR/$SCRIPT.err \
		&& printf "$SCRIPT $S\n" \
		|| (printf "$SCRIPT $F\n" \
		&& exit 1)
	fi
else
	echo "Skip test $SCRIPT"
fi
