#!/bin/sh
#
# Extract the dgsh syntax from the manual page and format it into HTML
#

sed -n '/^<dgsh_block/,/^\.fi/ {
	s/&/\&amp;/g
	s/</\&lt;/g
	s/>/\&gt;/g
	/^\.fi/d
	p
}' dgsh.1
