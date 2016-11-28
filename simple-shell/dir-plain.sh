ls -n |
dgsh-tee |
{{
	awk '!/^total/ {print $6, $7, $8, $1, sprintf("%8d", $5), $9}' &
	awk '{s += $5} END {printf("%d bytes", s)}' &
}} |
dgsh-tee
	
