{{
	sgsh-wrap cut -f2 $1 &

	sgsh-wrap cut -f1 $1 &
}} |
paste - -
