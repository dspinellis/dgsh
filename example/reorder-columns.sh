export SGSH_DOT_DRAW="$(basename $0 .sh).dot"

sgsh-tee |
{{
	cut -d , -f 5-6 - &

	cut -d , -f 2-4 - &
}} |
paste -d , - -
