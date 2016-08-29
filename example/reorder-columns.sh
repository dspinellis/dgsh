sgsh-tee |
{{
	sgsh-wrap cut -f2 - &

	sgsh-wrap cut -f1 - &
}} |
paste - -
