sgsh-tee |
{{
	sgsh-wrap cut -d , -f 5-6 - &

	sgsh-wrap cut -d , -f 2-4 - &
}} |
paste -d , - -
