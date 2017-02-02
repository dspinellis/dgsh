## Process this file with automake to produce Makefile.in

TESTS = check_negotiate
check_PROGRAMS = check_negotiate

check_negotiate_SOURCES = check_negotiate.c ../src/negotiate.h
check_negotiate_CFLAGS = @CHECK_CFLAGS@ -DUNIT_TESTING -DDEBUG
check_negotiate_LDADD = ../src/libdgsh.a @CHECK_LIBS@

