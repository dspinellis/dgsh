#!/bin/sh
# Produce statistics on the character count of the data received from
# standard input
sed 's/./&\
/g' |
sort |
uniq -c
