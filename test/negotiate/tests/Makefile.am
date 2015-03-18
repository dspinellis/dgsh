## Process this file with automake to produce Makefile.in

AM_CFLAGS = -DUNIT_TESTING
TESTS = check_negotiate
check_PROGRAMS = check_negotiate
check_negotiate_SOURCES = check_negotiate.c $(top_builddir)/src/sgsh-negotiate.h
check_negotiate_CFLAGS = @CHECK_CFLAGS@
check_negotiate_LDADD = $(top_builddir)/src/libnegotiate.la @CHECK_LIBS@

