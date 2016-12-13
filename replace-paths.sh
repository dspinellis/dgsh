#!/bin/sh
#
# Filter that replace the @-delimited paths with those based on the
# PREFIX environment variable
#

sed "
s|@bindir@|${PREFIX}/bin|g
s|@libexecdir@|${PREFIX}/libexec|g
"
