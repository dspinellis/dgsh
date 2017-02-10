#!/bin/sh
#
# Filter that replace the @-delimited paths with those based on the
# PREFIX environment variable
#

if [ "$1" = "" ] ; then
	sed "
	s|@dgshdir@|${PREFIX}/bin|g
	s|@bindir@|${PREFIX}/bin|g
	s|@libexecdir@|${PREFIX}/libexec|g
	"
else
	sed "
	s|@dgshdir@|PATH="`pwd`/../../build/libexec/dgsh:\$PATH" $1|g
	s|@bindir@|$1/bin|g
	s|@libexecdir@|$1/libexec|g
	"
fi
