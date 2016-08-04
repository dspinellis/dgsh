SGSH=..

$SGSH/sgsh-wrap ls -n | $SGSH/sgsh-tee | {{
	$SGSH/sgsh-wrap awk '!/^total/ {print $6, $7, $8, $1, sprintf("%8d", $5), $9}' &
	$SGSH/sgsh-wrap awk '{s += $5} END {printf("%d bytes", s)}' &
}} | $SGSH/sgsh-tee
	
