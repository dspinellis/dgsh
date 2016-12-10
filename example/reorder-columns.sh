tee |
{{
	cut -d , -f 5-6 - &

	cut -d , -f 2-4 - &
}} |
paste -d ,
