include ../../.config

if FREEBSD
CPOW = ../../unix-tools/cpow.c
else
CPOW =
endif

if LINUX
DGSH_ASSEMBLY_FILE=dgsh-elf.s
else
if DARWIN
DGSH_ASSEMBLY_FILE=dgsh-macho.s
endif
endif

lib_LIBRARIES = libdgsh.a
libdgsh_a_SOURCES = negotiate.c $(DGSH_ASSEMBLY_FILE)

include_HEADERS = dgsh.h

bin_PROGRAMS = dgsh-monitor dgsh-httpval dgsh-readval
bin_SCRIPTS = dgsh-merge-sum

man1_MANS = dgsh.1 dgsh-conc.1 dgsh-enumerate.1 dgsh-httpval.1 \
	    dgsh-merge-sum.1 dgsh-monitor.1 \
	    dgsh-parallel.1 dgsh-readval.1 dgsh-tee.1 dgsh-wrap.1 \
	    dgsh-writeval.1 perm.1

man3_MANS = dgsh_negotiate.3

libexec_PROGRAMS = dgsh-tee dgsh-writeval dgsh-readval dgsh-monitor \
		 dgsh-conc dgsh-wrap dgsh-enumerate dgsh-pecho \
		 dgsh-fft-input dgsh-w
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
dgsh_pecho_SOURCES = dgsh-pecho.c
dgsh_fft_input_SOURCES = dgsh-fft-input.c
dgsh_w_SOURCES = dgsh-w.c $(CPOW)

dgsh_readval_LDADD = libdgsh.a
dgsh_writeval_LDADD = libdgsh.a
dgsh_conc_LDADD = libdgsh.a
dgsh_wrap_LDADD = libdgsh.a
dgsh_tee_LDADD = libdgsh.a
dgsh_enumerate_LDADD = libdgsh.a
dgsh_pecho_LDADD = libdgsh.a
dgsh_fft_input_LDADD = libdgsh.a
dgsh_w_LDADD = libdgsh.a -lm

dgsh-parallel: dgsh-parallel.sh
	install $? $@

perm: perm.sh
	install $? $@

dgsh-merge-sum: dgsh-merge-sum.pl
	install $? $@

clean-local:
	-rm -rf dgsh-parallel perm degsh-merge-sum

build-install:
	mkdir -p ../../build/bin ../../build/libexec/dgsh
	cp $(bin_PROGRAMS) $(bin_SCRIPTS) ../../build/bin/
	cp $(libexec_PROGRAMS) $(libexec_SCRIPTS) ../../build/libexec/dgsh/
