#!/bin/sh
#
# Extract the dgsh syntax from the manual page and format it into HTML
#

sed -n '/^program : /,/^\.fi/ {
	s/&/\&amp;/g
	s/</\&lt;/g
	s/>/\&gt;/g
	/^\.fi/d
	p
}' dgsh.1
