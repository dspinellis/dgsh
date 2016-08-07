# Run from current directory as:
#   bin/bash --sgsh echo_echo_sgsh-tee.sh 2>err

{{
	sgsh-wrap echo hello &
	sgsh-wrap echo world &
}} | sgsh-tee
