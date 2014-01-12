#!/usr/local/bin/sgsh

scatter |{
	#-| scatter1 |store:S
	-| scatter2 |{
		-| toa |>/stream/a
		-| tob |>/stream/b
	|}
|} gather |{
	cat /stream/a /stream/b
|}
