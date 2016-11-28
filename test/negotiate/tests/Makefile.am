## Process this file with automake to produce Makefile.in

TESTS = check_negotiate
check_PROGRAMS = check_negotiate

check_negotiate_SOURCES = check_negotiate.c $(top_builddir)/src/dgsh-negotiate.h
check_negotiate_CFLAGS = @CHECK_CFLAGS@ -DUNIT_TESTING -DDEBUG
check_negotiate_LDADD = $(top_builddir)/src/libnegotiate.la $(top_builddir)/src/libdgshconc.la @CHECK_LIBS@

#check_conc_SOURCES = check_conc.c c $(top_builddir)/src/dgsh-internal-api.h
#check_conc_CFLAGS = @CHECK_CFLAGS@ -DUNIT_TESTING -DDEBUG
#check_conc_LDADD = $(top_builddir)/src/libdgshconc.la $(top_builddir)/src/libnegotiate.la @CHECK_LIBS@


