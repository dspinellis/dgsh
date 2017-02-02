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

install: cat tee
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
	../core-tools/src/replace-paths.sh <$? >$@

tee: tee.sh
	../core-tools/src/replace-paths.sh <$? >$@

test-clean-bash:
	-rm $(PSDIR)/*.outb

test-clean:
	-rm $(PSDIR)/*.out

test: test-bash

path:
	export PATH
	echo $(PATH)

test-bash: test-clean
	printf "$B\nBash tests:${EC}\n"
	
	$(DGSHPATH)/bash --dgsh -c ' \
		secho hello \
		| paste $(PSDIR)/world \
		> $(PSDIR)/secho_paste.outb' 2>$(PSDIR)/secho_paste.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/secho_paste.outb $(PSDIR)/secho_paste.success \
	&& printf "secho | paste $S\n" \
	|| (printf "secho | paste $F\n" \
	&& exit 1)

	$(DGSHPATH)/bash --dgsh -c ' \
		cat $(PSDIR)/f1s \
		| comm $(PSDIR)/f2s \
		| sort \
		| wc -l \
		| tr -d " " \
		> $(PSDIR)/wrap-cat_comm_sort.outb' \
		2>$(PSDIR)/wrap-cat_comm_sort.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/wrap-cat_comm_sort.outb \
		$(PSDIR)/wrap-cat_comm_sort.success \
	&& printf "dgsh-wrap cat | comm | sort $S\n" \
	|| (printf "dgsh-wrap cat | comm | sort $F\n" \
	&& exit 1)
	
	$(DGSHPATH)/bash --dgsh -c ' \
		comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		| wc -l \
		| tr -d " " > $(PSDIR)/comm_sort.outb' 2>$(PSDIR)/comm_sort.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/comm_sort.outb $(PSDIR)/comm_sort.success \
	&& printf "comm | sort $S\n" \
	|| (printf "comm | sort $F\n" \
	&& exit 1)
	
	$(DGSHPATH)/bash --dgsh -c ' \
		comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| paste \
		> $(PSDIR)/comm_paste.outb' 2>$(PSDIR)/comm_paste.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/comm_paste.outb \
		$(PSDIR)/comm_paste.success \
	&& printf "comm | paste $S\n" \
	|| (printf "comm | paste $F\n" \
	&& exit 1)
	
	$(DGSHPATH)/bash --dgsh -c ' \
		join $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		> $(PSDIR)/join_sort.outb' 2>$(PSDIR)/join_sort.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/join_sort.outb \
		$(PSDIR)/join_sort.success \
	&& printf "join | sort $S\n" \
	|| (printf "join | sort $F\n" \
	&& exit 1)
	
	# || instead of && in the following test because diff's exit 
	# status becomes the command's exit status
	$(DGSHPATH)/bash --dgsh -c ' \
		paste $(PSDIR)/f1s $(PSDIR)/f2s \
		| diff $(PSDIR)/f1s \
		> $(PSDIR)/paste_diff.outb' 2>$(PSDIR)/paste_diff.errb \
	&& sleep 0.01 \
	|| diff $(PSDIR)/paste_diff.outb $(PSDIR)/paste_diff.success \
	&& printf "paste | diff $S\n" \
	|| (printf "paste | diff $F\n" \
	&& exit 1)
	
	#ditto
	#$(DGSHPATH)/bash --dgsh -c ' \
	#	grep -v -w match $(PSDIR)/F $(PSDIR)/ff \
	#	| diff \
	#	> $(PSDIR)/grep_diff.outb' 2>$(PSDIR)/grep_diff.errb \
	#&& sleep 0.01 \
	#|| diff $(PSDIR)/grep_diff.outb $(PSDIR)/grep_diff.success-bash \
	#&& printf "grep | diff $S\n" \
	#|| (printf "grep | diff $F\n" \
	#&& exit 1)
	
	$(DGSHPATH)/bash --dgsh -c ' \
		grep -l -L match $(PSDIR)/ff $(PSDIR)/F \
		| comm \
		> $(PSDIR)/grep_comm.outb' 2>$(PSDIR)/grep_comm.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/grep_comm.outb $(PSDIR)/grep_comm.success-bash \
	&& printf "grep | comm $S\n" \
	|| (printf "grep | comm $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh -c ' \
		join $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		| diff $(PSDIR)/f3s \
		> $(PSDIR)/join_sort_diff.outb' 2>$(PSDIR)/join_sort_diff.errb \
	&& sleep 0.01 \
	|| diff $(PSDIR)/join_sort_diff.outb \
		$(PSDIR)/join_sort_diff.success \
	&& printf "join | sort | diff $S\n" \
	|| (printf "join | sort | diff $F\n" \
	&& exit 1)
	
	# No diff, just check execution exit status
	$(DGSHPATH)/bash --dgsh $(PSDIR)/dir-plain.sh \
		>$(PSDIR)/dir-plain.outb 2>$(PSDIR)/dir-plain.errb \
	&& printf "ls -n | {{ awk & awk & }} $S\n" \
	|| (printf "ls -n | {{ awk & awk & }} $F\n" \
	&& exit 1)
	
	# ditto
	# `date`: Check that command substitution
	# does not mess pipe substitution
	$(DGSHPATH)/bash --dgsh -c ' \
		{{ \
			secho match & \
			secho "not `date`" & \
		}} \
		| grep -F -h match \
	> $(PSDIR)/secho_secho_fgrep.outb' 2>$(PSDIR)/secho_secho_fgrep.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/secho_secho_fgrep.outb \
		$(PSDIR)/secho_secho_fgrep.success \
	&& printf "{{ secho & secho & }} | grep -F $S\n" \
	|| (printf "{{ secho & secho & }} | grep -F $F\n" \
	&& exit 1)
	
	$(DGSHPATH)/bash --dgsh -c ' \
		tee <$(PSDIR)/hello \
		| {{ \
			diff $(PSDIR)/world & \
			comm $(PSDIR)/hello & \
		}} > $(PSDIR)/tee-copy_diff_comm.outb' \
		2>$(PSDIR)/tee-copy_diff_comm.errb \
	&& sleep 0.01 \
	&& (diff $(PSDIR)/tee-copy_diff_comm.outb \
			$(PSDIR)/tee-copy_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-copy_diff_comm.outb \
			$(PSDIR)/tee-copy_diff_comm.success2 >/dev/null) \
	&& printf "tee | {{ diff & comm & }} $S\n" \
	|| (printf "tee | {{ diff & comm & }} $F\n" \
	&& exit 1)
	
	# ditto
	#$(DGSHPATH)/bash --dgsh -c ' \
	#	grep -l -L -w -v match $(PSDIR)/ff $(PSDIR)/F \
	#	| {{ \
	#		diff & \
	#		comm & \
	#	}} > $(PSDIR)/grep_diff_comm.outb' \
	#	2>$(PSDIR)/grep_diff_comm.errb \
	#&& sleep 0.01 \
	#&& (diff $(PSDIR)/grep_diff_comm.outb \
	#	$(PSDIR)/grep_diff_comm.success1-bash >/dev/null \
	#    || diff $(PSDIR)/grep_diff_comm.outb \
	#	$(PSDIR)/grep_diff_comm.success2-bash >/dev/null \
	#    || diff $(PSDIR)/grep_diff_comm.outb \
	#	$(PSDIR)/grep_diff_comm.success3-bash >/dev/null \
	#    || diff $(PSDIR)/grep_diff_comm.outb \
	#	$(PSDIR)/grep_diff_comm.success4-bash >/dev/null) \
	#&& printf "grep | {{ diff & comm & }} $S\n" \
	#|| (printf "grep | {{ diff & comm & }} $F\n" \
	#&& exit 1)
	
	$(DGSHPATH)/bash --dgsh -c ' \
		comm $(PSDIR)/f4ss $(PSDIR)/f5ss \
		| {{ \
			paste $(PSDIR)/p1 & \
			join $(PSDIR)/j2 & \
			diff $(PSDIR)/d3 & \
		}} > $(PSDIR)/comm_paste_join_diff.outb' \
		2>$(PSDIR)/comm_paste_join_diff.errb \
	&& sleep 0.01 \
	&& (diff $(PSDIR)/comm_paste_join_diff.outb \
		$(PSDIR)/comm_paste_join_diff.success1 >/dev/null \
	|| diff $(PSDIR)/comm_paste_join_diff.outb \
		$(PSDIR)/comm_paste_join_diff.success2 > /dev/null) \
	&& printf "comm | {{ paste & join & diff & }} $S\n" \
	|| (printf "comm | {{ paste & join & diff & }} $F\n" \
	&& exit 1)
	
	$(DGSHPATH)/bash --dgsh -c ' \
		{{ \
			sort $(PSDIR)/f4s 2>$(PSDIR)/f4s.errb & \
			sort $(PSDIR)/f5s 2>$(PSDIR)/f5s.errb & \
		}} \
		| comm \
		> $(PSDIR)/sort_sort_comm.outb' \
		2>$(PSDIR)/sort_sort_comm.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/sort_sort_comm.outb \
		$(PSDIR)/sort_sort_comm.success \
	&& printf "{{ sort & sort & }} | comm $S\n" \
	|| (printf "{{ sort & sort & }} | comm $F\n" \
	&& exit 1)
	
	$(DGSHPATH)/bash --dgsh -c ' \
		{{ \
			sort $(PSDIR)/f4s & \
			sort $(PSDIR)/f5s & \
		}} \
		| comm \
		| {{ \
			paste $(PSDIR)/p1 & \
			join $(PSDIR)/j2 & \
			diff $(PSDIR)/d3 & \
		}} > $(PSDIR)/sort_sort_comm_paste_join_diff.outb' \
			2>$(PSDIR)/sort_sort_comm_paste_join_diff.errb \
	&& sleep 0.01 \
	&& (diff $(PSDIR)/sort_sort_comm_paste_join_diff.outb \
		$(PSDIR)/sort_sort_comm_paste_join_diff.success1 >/dev/null \
	|| diff $(PSDIR)/sort_sort_comm_paste_join_diff.outb \
		$(PSDIR)/sort_sort_comm_paste_join_diff.success2 >/dev/null) \
	&& printf "{{ sort & sort & }} | comm | {{ paste & join & diff & }} $S\n" \
	|| (printf "{{ sort & sort & }} | comm | {{ paste & join & diff & }} $F\n" \
	&& exit 1)
	
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
	
test-pseudo: test-clean
	printf "$B\nPseudo shell tests:${EC}\n"
	
	cd $(PSDIR) && \
	python2 simple-shell.py secho_paste.dgsh PREFIX=$(DGSHPATH) OUT=secho_paste.out \
		2>$(PSDIR)/secho_paste.err \
	&& sleep 0.01 \
	&& diff secho_paste.out secho_paste.success \
	&& printf "secho | paste $S\n" \
	|| (printf "secho | paste $F\n" \
	&& exit 1)

	cd $(PSDIR) && \
	python2 simple-shell.py wrap-cat_comm_sort.dgsh PREFIX=$(DGSHPATH) OUT=wrap-cat_comm_sort.out \
		2>$(PSDIR)/wrap-cat_comm_sort.err \
	&& sleep 0.01 \
	&& diff wrap-cat_comm_sort.out wrap-cat_comm_sort.success \
	&& printf "dgsh-wrap cat | comm | sort $S\n" \
	|| (printf "dgsh-wrap cat | comm | sort $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py comm_sort.dgsh PREFIX=$(DGSHPATH) OUT=comm_sort.out \
		2>$(PSDIR)/comm_sort.err \
	&& diff comm_sort.out comm_sort.success \
	&& printf "comm | sort $S\n" \
	|| (printf "comm | sort $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py comm_paste.dgsh PREFIX=$(DGSHPATH) OUT=comm_paste.out \
		2>$(PSDIR)/comm_paste.err \
	&& sleep 0.01 \
	&& diff comm_paste.out comm_paste.success \
	&& printf "comm | paste $S\n" \
	|| (printf "comm | paste $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py join_sort.dgsh PREFIX=$(DGSHPATH) OUT=join_sort.out \
		2>$(PSDIR)/join_sort.err \
	&& sleep 0.01 \
	&& diff join_sort.out join_sort.success \
	&& printf "join | sort $S\n" \
	|| (printf "join | sort $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py paste_diff.dgsh PREFIX=$(DGSHPATH) OUT=paste_diff.out \
		2>$(PSDIR)/paste_diff.err \
	&& sleep 0.01 \
	&& diff paste_diff.out paste_diff.success \
	&& printf "paste | diff $S\n" \
	|| (printf "paste | diff $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py grep_diff.dgsh PREFIX=$(DGSHPATH) OUT=grep_diff.out \
		2>$(PSDIR)/grep_diff.err \
	&& sleep 0.01 \
	&& diff grep_diff.out grep_diff.success \
	&& printf "grep | diff $S\n" \
	|| (printf "grep | diff $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py grep_comm.dgsh PREFIX=$(DGSHPATH) OUT=grep_comm.out \
		2>$(PSDIR)/grep_comm.err \
	&& diff grep_comm.out grep_comm.success \
	&& printf "grep | comm $S\n" \
	|| (printf "grep | comm $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py join_sort_diff.dgsh PREFIX=$(DGSHPATH) OUT=join_sort_diff.out \
		2>$(PSDIR)/join_sort_diff.err \
	&& sleep 0.01 \
	&& diff join_sort_diff.out join_sort_diff.success \
	&& printf "join | sort | diff $S\n" \
	|| (printf "join | sort | diff $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py dir-plain.dgsh PREFIX=$(DGSHPATH) OUT=dir-plain.out \
		2>$(PSDIR)/dir-plain.err \
	&& sleep 0.01 \
	&& printf "ls -n | {{ awk & awk & }} $S\n" \
	|| (printf "ls -n | {{ awk & awk & }} $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py secho_secho_fgrep.dgsh PREFIX=$(DGSHPATH) OUT=secho_secho_fgrep.out \
		2>$(PSDIR)/secho_secho_fgrep.err \
	&& sleep 0.01 \
	&& diff $(PSDIR)/secho_secho_fgrep.out \
		$(PSDIR)/secho_secho_fgrep.success \
	&& printf "{{ secho & secho & }} | grep -F $S\n" \
	|| (printf "{{ secho & secho & }} | grep -F $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py tee-copy_diff_comm.dgsh PREFIX=$(DGSHPATH) OUT=tee-copy_diff_comm.out \
		2>$(PSDIR)/tee-copy_diff_comm.err \
	&& sleep 0.01 \
	&& (diff $(PSDIR)/tee-copy_diff_comm.out \
			$(PSDIR)/tee-copy_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-copy_diff_comm.out \
			$(PSDIR)/tee-copy_diff_comm.success2 >/dev/null) \
	&& printf "tee | {{ diff & comm & }} $S\n" \
	|| (printf "tee | {{ diff & comm & }} $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py grep_diff_comm.dgsh PREFIX=$(DGSHPATH) OUT=grep_diff_comm.out \
		2>$(PSDIR)/grep_diff_comm.err \
	&& sleep 0.01 \
	&& ($(STD)/diff grep_diff_comm.out grep_diff_comm.success1 >/dev/null \
	    || $(STD)/diff grep_diff_comm.out grep_diff_comm.success2 >/dev/null) \
	    || $(STD)/diff grep_diff_comm.out grep_diff_comm.success3 >/dev/null \
	    || $(STD)/diff grep_diff_comm.out grep_diff_comm.success4 >/dev/null \
	&& printf "grep | {{ diff & comm & }} $S\n" \
	|| (printf "grep | {{ diff & comm & }} $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py sort_sort_comm.dgsh PREFIX=$(DGSHPATH) OUT=sort_sort_comm.out \
		2>$(PSDIR)/sort_sort_comm.err \
	&& sleep 0.01 \
	&& $(STD)/diff sort_sort_comm.out sort_sort_comm.success \
	&& printf "{{ sort & sort & }} | comm $S\n" \
	|| (printf "{{ sort & sort & }} | comm $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py comm_paste_join_diff.dgsh PREFIX=$(DGSHPATH) \
		OUT=comm_paste_join_diff.out 2>$(PSDIR)/comm_paste_join_diff.err \
	&& sleep 0.01 \
	&& (diff comm_paste_join_diff.out \
		comm_paste_join_diff.success1 >/dev/null \
	|| diff comm_paste_join_diff.out \
		comm_paste_join_diff.success2 > /dev/null) \
	&& printf "comm | {{ paste & join & diff & }} $S\n" \
	|| (printf "comm | {{ paste & join & diff & }} $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py \
		sort_sort_comm_paste_join_diff.dgsh PREFIX=$(DGSHPATH) \
		OUT=sort_sort_comm_paste_join_diff.out \
		2>$(PSDIR)/sort_sort_comm_paste_join_diff.err \
	&& sleep 0.01 \
	&& (diff sort_sort_comm_paste_join_diff.out \
		sort_sort_comm_paste_join_diff.success1 >/dev/null \
	|| diff sort_sort_comm_paste_join_diff.out \
		sort_sort_comm_paste_join_diff.success2 >/dev/null) \
	&& printf "{{ sort & sort & }} | comm | {{ paste & join & diff & }} $S\n" \
	|| (printf "{{ sort & sort & }} | comm | {{ paste & join & diff & }} $F\n" \
	&& exit 1)

clean:
	$(MAKE) -C bash clean
	rm $(PREFIX)/bin/dgsh
	$(MAKE) -C coreutils clean
	$(MAKE) -C diffutils clean
	$(MAKE) -C grep clean
