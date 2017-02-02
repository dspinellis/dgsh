include ../../.config

lib_LIBRARIES = libdgsh.a
libdgsh_a_SOURCES = negotiate.c

include_HEADERS = dgsh.h

bin_PROGRAMS = dgsh-monitor dgsh-httpval dgsh-readval
bin_SCRIPTS = dgsh-merge-sum

man1_MANS = dgsh.1 dgsh-conc.1 dgsh-enumerate.1 dgsh-httpval.1 \
	    dgsh-merge-sum.1 dgsh-monitor.1 \
	    dgsh-parallel.1 dgsh-readval.1 dgsh-tee.1 dgsh-wrap.1 \
	    dgsh-writeval.1 perm.1

man3_MANS = dgsh_negotiate.3

libexec_PROGRAMS = dgsh-tee dgsh-writeval dgsh-readval dgsh-monitor \
		 dgsh-conc dgsh-wrap dgsh-enumerate
libexec_SCRIPTS = dgsh-parallel perm
libexecdir = $(prefix)/libexec/dgsh

dgsh_monitor_SOURCES = dgsh-monitor.c
dgsh_httpval_SOURCES = dgsh-httpval.c kvstore.c
dgsh_readval_SOURCES = dgsh-readval.c kvstore.c
dgsh_tee_SOURCES = dgsh-tee.c
dgsh_writeval_SOURCES = dgsh-writeval.c
dgsh_conc_SOURCES = dgsh-conc.c
dgsh_wrap_SOURCES = dgsh-wrap.c
dgsh_enumerate_SOURCES = dgsh-enumerate.c

dgsh_readval_LDADD = libdgsh.a
dgsh_writeval_LDADD = libdgsh.a
dgsh_conc_LDADD = libdgsh.a
dgsh_wrap_LDADD = libdgsh.a
dgsh_tee_LDADD = libdgsh.a
dgsh_enumerate_LDADD = libdgsh.a

dgsh-parallel: dgsh-parallel.sh
	cp $? $@

perm: perm.sh
	./replace-paths.sh <$? >$@
	chmod 755 $@

dgsh-merge-sum: dgsh-merge-sum.pl
	./replace-paths.sh <$? >$@
	chmod 755 $@

charcount: charcount.sh
	install charcount.sh charcount

