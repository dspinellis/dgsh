export SGSH_DOT_DRAW="$(basename $0 .sh).draw"

PSDIR=$1

cp $PSDIR/results $PSDIR/res

# Sort result files
{{
	sort $PSDIR/f4s &
	sort $PSDIR/f5s &
}} |
# Remove noise
comm - - |
{{
	# Paste to master results file
	paste - $PSDIR/res > results &

	# Join with selected records
	join - $PSDIR/top > top_results &

	# Diff from previous results file
	diff - $PSDIR/last > diff_last &
}}
