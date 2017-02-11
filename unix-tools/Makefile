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

PREFIX?=/usr/local/dgsh
DGSHPATH=$(PREFIX)/libexec/dgsh

DGSHDIR=../core-tools/src
PSDIR=../simple-shell
XDIR=../example
# TOOLDIR
TD=unix-tools
STD=/usr/bin
SCRIPTDIR=tool_scripts

PATHS= -I$(DGSHDIR)
LIBS= $(DGSHDIR)/libdgsh.a

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
	cd bash && ./configure --prefix=$(PREFIX) --bindir=$(DGSHPATH)
	cd coreutils && ./bootstrap && ../cygwin-sys-select-patch.sh && \
	./configure --prefix=$(PREFIX) \
		--bindir=$(DGSHPATH) \
		--enable-no-install-program="$(DISABLE_COREUTILS_PROGRAMS)"
	cd diffutils && ./bootstrap && ./configure --prefix=$(PREFIX) --bindir=$(DGSHPATH)
	cd grep && ./bootstrap && ./configure --prefix=$(PREFIX) --bindir=$(DGSHPATH)

make: w
	$(MAKE) -C bash CFLAGS="$(CFLAGS)"
	$(MAKE) -C coreutils
	$(MAKE) -C diffutils
	$(MAKE) -C grep
	$(CC) $(CFLAGS) $(PATHS) simple_echo.c -o secho $(LIBS)
	$(CC) $(CFLAGS) $(PATHS) pecho.c -o pecho $(LIBS)
	$(CC) $(CFLAGS) $(PATHS) fft-input.c -o fft-input $(LIBS)

w: w.c $(LIBS)
ifeq ($(shell uname),FreeBSD)
	$(CC) $(CFLAGS) $(PATHS) w.c cpow.c -o w $(LIBS) -lm
else
	$(CC) $(CFLAGS) $(PATHS) w.c -o w $(LIBS) -lm
endif

build-install: cat tee w make
	cp bash/bash ../build/libexec/dgsh/
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
	cp -p secho pecho fft-input w cat tee ../build/libexec/dgsh/
	./install-wrapped.sh ../build

install: cat tee w make
	$(MAKE) -C bash install
	rm -f $(DESTDIR)$(PREFIX)/bin/dgsh
	ln $(DESTDIR)$(DGSHPATH)/bash $(DESTDIR)$(PREFIX)/bin/dgsh || install $(DESTDIR)$(DGSHPATH)/bash $(DESTDIR)$(PREFIX)/bin/dgsh
	$(MAKE) -C coreutils install
	$(MAKE) -C diffutils install
	$(MAKE) -C grep install
	# Install last to overwrite standard tools of coreutils
	install cat $(DESTDIR)$(DGSHPATH)
	install tee $(DESTDIR)$(DGSHPATH)
	install secho $(DESTDIR)$(DGSHPATH)
	install pecho $(DESTDIR)$(DGSHPATH)
	install fft-input $(DESTDIR)$(DGSHPATH)
	install w $(DESTDIR)$(DGSHPATH)
	./install-wrapped.sh

cat: cat.sh
	install $? $@

tee: tee.sh
	install $? $@


test: test-bash

path:
	export PATH
	echo $(PATH)

test-bash:
	-rm $(PSDIR)/*.out $(PSDIR)/*.outb
	printf "$B\nBash tests:${EC}\n"
	
	./run_simple_test.sh $(PSDIR) secho_paste \
		"secho hello \
		| paste $(PSDIR)/world"

	./run_simple_test.sh $(PSDIR) wrap-cat_comm_sort \
		"cat $(PSDIR)/f1s \
		| comm $(PSDIR)/f2s \
		| sort \
		| wc -l \
		| tr -d \" \""
	
	./run_simple_test.sh $(PSDIR) comm_sort \
		"comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		| wc -l \
		| tr -d \" \""
	
	./run_simple_test.sh $(PSDIR) comm_paste \
		"comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| paste"
	
	./run_simple_test.sh $(PSDIR) join_sort \
		"join $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort"
	
	./run_simple_test.sh $(PSDIR) paste_diff \
		"paste $(PSDIR)/f1s $(PSDIR)/f2s \
		| diff $(PSDIR)/f1s \
		| cat"
	
	#./run_simple_test.sh $(DGSHPATH) $(PSDIR) grep_diff \
	#	"grep -v -w match $(PSDIR)/F $(PSDIR)/ff \
	#	| diff"
	
	./run_simple_test.sh $(PSDIR) grep_comm \
		"grep -l -L match $(PSDIR)/ff $(PSDIR)/F \
		| comm"
	
	./run_simple_test.sh $(PSDIR) join_sort_diff \
		"join $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		| diff $(PSDIR)/f3s \
		| cat"
	
	# `date`: Check that command substitution
	# does not mess pipe substitution
	./run_simple_test.sh $(PSDIR) secho_secho_fgrep \
		"{{ \
			secho match & \
			secho \"not `date`\" & \
		}} \
		| grep -F -h match"
	
	./run_simple_test.sh $(PSDIR) tee-copy_diff_comm \
		"tee <$(PSDIR)/hello \
		| {{ \
			diff $(PSDIR)/world & \
			comm $(PSDIR)/hello & \
		}} \
		| cat"
	
	# ditto
	#./run_simple_test.sh $(DGSHPATH) $(PSDIR) grep_diff_comm \
	#	"grep -l -L -w -v match $(PSDIR)/ff $(PSDIR)/F \
	#	| {{ \
	#		diff & \
	#		comm & \
	#	}}"
	
	./run_simple_test.sh $(PSDIR) comm_paste_join_diff \
		"comm $(PSDIR)/f4ss $(PSDIR)/f5ss \
		| {{ \
			paste $(PSDIR)/p1 & \
			join $(PSDIR)/j2 & \
			diff $(PSDIR)/d3 & \
		}} \
		| cat"
	
	./run_simple_test.sh $(PSDIR) sort_sort_comm \
		"{{ \
			sort $(PSDIR)/f4s 2>$(PSDIR)/f4s.errb & \
			sort $(PSDIR)/f5s 2>$(PSDIR)/f5s.errb & \
		}} \
		| comm"
	
	./run_simple_test.sh $(PSDIR) sort_sort_comm_paste_join_diff \
		"{{ \
			sort $(PSDIR)/f4s & \
			sort $(PSDIR)/f5s & \
		}} \
		| comm \
		| {{ \
			paste $(PSDIR)/p1 & \
			join $(PSDIR)/j2 & \
			diff $(PSDIR)/d3 & \
		}} \
		| cat"
	
	printf " $BPaper examples under ../example:${EC}\n"

	# No diff, just check execution exit status
	./run_test.sh $(PSDIR) $(XDIR)/dir.sh 0
	./run_test.sh $(PSDIR) $(XDIR)/reorder-columns.sh pipe $(PSDIR)/data.csv
	./run_test.sh $(PSDIR) $(XDIR)/set-operations.sh file $(PSDIR)
	./run_test.sh $(PSDIR) $(XDIR)/compress-compare.sh pipe Readme.md
	./run_test.sh $(PSDIR) $(XDIR)/commit-stats.sh
	./run_test.sh $(PSDIR) $(XDIR)/uniform-5x5.sh
	./run_test.sh $(PSDIR) $(XDIR)/duplicate-files.sh file coreutils
	./run_test.sh $(PSDIR) $(XDIR)/spell-highlight.sh pipe Readme.md
	./run_test.sh $(PSDIR) $(XDIR)/static-functions.sh file coreutils
	./run_test.sh $(PSDIR) $(XDIR)/word-properties.sh file Readme.md
	-rm -rf grep2 grep-p100
	cp -R grep grep2 && \
	cp -R grep2 grep-p100 && \
	cd grep-p100 && git checkout HEAD~100 >/dev/null && \
	cd ../grep2 && git checkout master && cd ..
	./run_test.sh $(PSDIR) $(XDIR)/map-hierarchy.sh file grep2 grep-p100 grepp
	./run_test.sh $(PSDIR) $(XDIR)/text-properties.sh pipe Readme.md
	cd coreutils && \
	../run_test.sh ../$(PSDIR) ../$(XDIR)/committer-plot.sh -- pnmtopng pamscale pgmmorphconv
	./run_test.sh $(PSDIR) $(XDIR)/code-metrics.sh file coreutils
	./run_test.sh $(PSDIR) $(XDIR)/web-log-report.sh pipe $(PSDIR)/access.log
	./run_test.sh $(PSDIR) $(XDIR)/fft-block8.sh file $(PSDIR)/fft-input.dat
	./run_test.sh $(PSDIR) $(XDIR)/ft2d.sh -- sfimag sfgrey sfspike sfput sfmath \
					sfspray sffft1 sffft3 sfcat sflmostretch \
					sfreverse sfwindow vppen sfsmooth
	# Requires file fid in $(PSDIR). See $(XDIR)/NMRPipe.sh
	./run_test.sh $(PSDIR) $(XDIR)/NMRPipe.sh file $(PSDIR)/fid -- nmrPipe addNMR

clean:
	$(MAKE) -C bash clean
	$(MAKE) -C coreutils clean
	$(MAKE) -C diffutils clean
	$(MAKE) -C grep clean
