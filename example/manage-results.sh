PSDIR=../simple-shell

# Sort result files
{{
	sort $(PSDIR)/f4s &
	sort $(PSDIR)/f5s &
}}
# Find common records
| comm - - |
{{
	# Paste to master results file
	paste - $(PSDIR)/p1 &

	# Join with top records
	join - $(PSDIR)/j2 &

	# Diff from previous results file
	diff - $(PSDIR)/d3 &
}}
