include ../.config

# All but ginstall comm join paste sort
# ginstall, renamed from install, is used to install the programs
DISABLE_COREUTILS_PROGRAMS=\
who users uptime stty stdbuf pinky nice install hostid df chroot chcon \
cat basename base64 base32 [ expr expand env echo du dirname dircolors \
dir dd date csplit cp cksum chown chmod chgrp nproc nl mv \
mktemp mknod mkfifo mkdir md5sum ls logname ln link kill id head \
groups fold fmt false factor sha512sum sha384sum sha256sum sha224sum \
sha1sum seq runcon rmdir rm realpath readlink pwd ptx printf printenv \
pr pathchk od numfmt nohup unexpand uname tty tsort truncate true \
tr touch timeout test tee tail tac sync sum stat split sleep shuf \
shred yes whoami wc vdir unlink uniq

FID=http://www.bmrb.wisc.edu/ftp/pub/bmrb/timedomain/bmr6443/timedomain_data/c13-hsqc/june11-se-6426-CA.fid/fid 

PREFIX?=/usr/local/dgsh
DGSHPATH=$(PREFIX)/libexec/dgsh

DGSHDIR=../core-tools/src
PSDIR=../simple-shell
EGDIR=../example
MANDUMPDIR=/var/tmp/dgsh-share-duplicates
# TOOLDIR
TD=unix-tools
STD=/usr/bin
SCRIPTDIR=tool_scripts

PATHS= -I$(DGSHDIR)

# Color
GR=\033[0;32m	# Green
R=\033[0;31m	# Red
B=\033[0;34m	# Blue
EC=\033[0m	# End color
S=${GR}successful${EC}
F=${R}failed${EC}

ifdef DEBUG
CFLAGS+=-DDEBUG
endif

OS = $(shell uname -s)
ifeq ($(OS), Linux)
   BZIP2LOCATION=/bin/bzip2
   GZIPLOCATION=/bin/gzip
   SEDLOCATION=/bin/sed
   XZLOCATION=/usr/bin/xz
else
   ifeq ($(OS), Darwin)
       BZIP2LOCATION=/usr/bin/bzip2
       GZIPLOCATION=/usr/bin/gzip
       SEDLOCATION=/usr/bin/sed
       XZLOCATION=/usr/local/bin/xz
   endif
endif

# If not cloned repo with --recursive use this target to
# clone the submodule repos
get-submodules:
	cd .. && git submodule update --init --recursive --depth=5000 \
		$(TD)/bash \
		$(TD)/coreutils \
		$(TD)/diffutils \
		$(TD)/grep

configure:
	cd bash && ./configure --prefix=$(PREFIX) --bindir=$(DGSHPATH) \
		--docdir=$(MANDUMPDIR) \
		--mandir=$(MANDUMPDIR) \
		--infodir=$(MANDUMPDIR) \
		--localedir=$(MANDUMPDIR)
	cd coreutils && ./bootstrap && ../cygwin-sys-select-patch.sh && \
	./configure --prefix=$(PREFIX) \
		--bindir=$(DGSHPATH) \
		--enable-no-install-program="$(DISABLE_COREUTILS_PROGRAMS)" \
		--docdir=$(MANDUMPDIR) \
		--mandir=$(MANDUMPDIR) \
		--infodir=$(MANDUMPDIR) \
		--localedir=$(MANDUMPDIR)
	cd diffutils && ./bootstrap && ./configure --prefix=$(PREFIX) \
		--bindir=$(DGSHPATH) \
		--docdir=$(MANDUMPDIR) \
		--mandir=$(MANDUMPDIR) \
		--infodir=$(MANDUMPDIR) \
		--localedir=$(MANDUMPDIR) \
		SHELL=`which bash`
	cd grep && ./bootstrap && ./configure --prefix=$(PREFIX) \
		--bindir=$(DGSHPATH) \
		--docdir=$(MANDUMPDIR) \
		--mandir=$(MANDUMPDIR) \
		--infodir=$(MANDUMPDIR) \
		--localedir=$(MANDUMPDIR)

make:
	$(MAKE) -C bash CFLAGS="$(CFLAGS)"
	$(MAKE) -C coreutils
	$(MAKE) -C diffutils
	$(MAKE) -C grep

build-install: cat tee make
	mkdir -p ../build/libexec/dgsh ../build/bin
	cp bash/bash ../build/libexec/dgsh/
	rm -f ../build/bin/dgsh
	ln bash/bash ../build/bin/dgsh
	cp coreutils/src/comm ../build/libexec/dgsh/
	cp coreutils/src/cut ../build/libexec/dgsh/
	cp coreutils/src/join ../build/libexec/dgsh/
	cp coreutils/src/paste ../build/libexec/dgsh/
	cp coreutils/src/sort ../build/libexec/dgsh/
	cp diffutils/src/diff ../build/libexec/dgsh/
	cp grep/src/grep ../build/libexec/dgsh/
	cp grep/src/egrep ../build/libexec/dgsh/
	cp grep/src/fgrep ../build/libexec/dgsh/
	chmod 755 tee cat
	cp -p cat tee ../build/libexec/dgsh/
	./install-wrapped.sh $$(cd .. ; pwd)/build

install: cat tee make
	$(MAKE) -C bash install
	rm -f $(DESTDIR)$(PREFIX)/bin/dgsh
	ln $(DESTDIR)$(DGSHPATH)/bash $(DESTDIR)$(PREFIX)/bin/dgsh || install $(DESTDIR)$(DGSHPATH)/bash $(DESTDIR)$(PREFIX)/bin/dgsh
	$(MAKE) -C coreutils install
	$(MAKE) -C diffutils install
	$(MAKE) -C grep install
	# Install last to overwrite standard tools of coreutils
	install cat $(DESTDIR)$(DGSHPATH)
	install tee $(DESTDIR)$(DGSHPATH)
	./install-wrapped.sh
	rm -rf $(DESTDIR)$(MANDUMPDIR)

cat: cat.sh
	install $? $@

tee: tee.sh
	install $? $@


test: test-bash

path:
	export PATH
	echo $(PATH)

../simple-shell/fid:
	wget -O $@ $(FID) || curl $(FID) >$@

test-bash: ../simple-shell/fid

	rm -f $(PSDIR)/*.out $(PSDIR)/*.outb
	printf "$B\nBash tests:${EC}\n"

	./run_all_simple_tests.sh $(PSDIR)
	
	printf " $BPaper examples under ../example:${EC}\n"

	# No diff, just check execution exit status
	./run_test.sh $(PSDIR) $(EGDIR)/dir.sh 0
	./run_test.sh $(PSDIR) $(EGDIR)/reorder-columns.sh pipe $(PSDIR)/data.csv
	./run_test.sh $(PSDIR) $(EGDIR)/set-operations.sh file $(PSDIR)
	./run_test.sh $(PSDIR) $(EGDIR)/compress-compare.sh pipe Readme.md
	./run_test.sh $(PSDIR) $(EGDIR)/commit-stats.sh
	./run_test.sh $(PSDIR) $(EGDIR)/uniform-5x5.sh
	./run_test.sh $(PSDIR) $(EGDIR)/duplicate-files.sh file coreutils
	./run_test.sh $(PSDIR) $(EGDIR)/spell-highlight.sh pipe Readme.md
	./run_test.sh $(PSDIR) $(EGDIR)/static-functions.sh file coreutils
	./run_test.sh $(PSDIR) $(EGDIR)/word-properties.sh file Readme.md
	-rm -rf grep2 grep-p100
	cp -R grep grep2 && \
	cp -R grep2 grep-p100 && \
	cd grep-p100 && git checkout HEAD~100 >/dev/null && \
	cd ../grep2 && git checkout master && cd ..
	./run_test.sh $(PSDIR) $(EGDIR)/map-hierarchy.sh file grep2 grep-p100 grepp
	./run_test.sh $(PSDIR) $(EGDIR)/text-properties.sh pipe Readme.md
	cd coreutils && \
	../run_test.sh ../$(PSDIR) ../$(EGDIR)/committer-plot.sh -- pnmtopng pamscale pgmmorphconv
	DGSH_TIMEOUT=20 ./run_test.sh $(PSDIR) $(EGDIR)/code-metrics.sh file coreutils
	KVSTORE_RETRY_LIMIT=30 DGSH_TIMEOUT=30 ./run_test.sh $(PSDIR) $(EGDIR)/web-log-report.sh pipe $(PSDIR)/access.log
	./run_test.sh $(PSDIR) $(EGDIR)/fft-block8.sh file $(PSDIR)/fft-input.dat
	./run_test.sh $(PSDIR) $(EGDIR)/ft2d.sh -- sfimag sfgrey sfspike sfput sfmath \
					sfspray sffft1 sffft3 sfcat sflmostretch \
					sfreverse sfwindow vppen sfsmooth
	# Requires file fid in $(PSDIR). See $(EGDIR)/NMRPipe.sh
	./run_test.sh $(PSDIR) $(EGDIR)/NMRPipe.sh file $(PSDIR)/fid -- nmrPipe addNMR

clean:
	$(MAKE) -C bash clean
	$(MAKE) -C coreutils clean
	$(MAKE) -C diffutils clean
	$(MAKE) -C grep clean
