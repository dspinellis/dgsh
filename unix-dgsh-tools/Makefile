PREFIX?=/usr/local/dgsh
DGSHPATH=$(PREFIX)/bin

DGSHDIR=..
PSDIR=../simple-shell
XDIR=../example
# TOOLDIR
TD=unix-dgsh-tools
STD=/usr/bin
SCRIPTDIR=tool_scripts

CFLAGS+=-Wall
PATHS= -I.. -L..
LIBS= -ldgsh_negotiate

# Color
GR=\033[0;32m	# Green
R=\033[0;31m	# Red
B=\033[0;34m	# Blue
EC=\033[0m	# End color
S=${GR}successful${EC}
F=${R}failed${EC}

ifdef DEBUG
DEBUGFLAG=-DDEBUG
CFLAGS+=-DDEBUG
else
DEBUGFLAG=
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
	cd bash && ./configure --prefix=$(PREFIX)
	cd coreutils && ./bootstrap && ./configure --prefix=$(PREFIX)
	cd diffutils && ./bootstrap && ./configure --prefix=$(PREFIX)
	cd grep && ./bootstrap && ./configure --prefix=$(PREFIX)

make:
	$(MAKE) -C bash CFLAGS=$(DEBUGFLAG)
	$(MAKE) -C coreutils
	$(MAKE) -C diffutils
	$(MAKE) -C grep
	$(CC) $(CFLAGS) $(PATHS) simple_echo.c -o secho $(LIBS)
	$(CC) $(CFLAGS) $(PATHS) pecho.c -o pecho $(LIBS)
	$(CC) $(CFLAGS) $(PATHS) fft-input.c -o fft-input $(LIBS)
	$(CC) $(CFLAGS) $(PATHS) w.c -o w $(LIBS) -lm
	echo "#!/usr/local/dgsh/bin/dgsh-wrap $(BZIP2LOCATION)" > $(SCRIPTDIR)/bzip2
	echo "#!/usr/local/dgsh/bin/dgsh-wrap $(GZIPLOCATION)" > $(SCRIPTDIR)/gzip
	echo "#!/usr/local/dgsh/bin/dgsh-wrap $(SEDLOCATION)" > $(SCRIPTDIR)/sed
	echo "#!/usr/local/dgsh/bin/dgsh-wrap $(XZLOCATION)" > $(SCRIPTDIR)/xz

install:
	$(MAKE) -C bash install
	$(MAKE) -C coreutils install
	$(MAKE) -C diffutils install
	$(MAKE) -C grep install
	# Install last to overwrite standard tools of coreutils
	install $(DGSHDIR)/dgsh-tee $(PREFIX)/bin/tee
	install $(DGSHDIR)/dgsh-tee $(PREFIX)/bin/cat
	install secho $(PREFIX)/bin
	install pecho $(PREFIX)/bin
	install fft-input $(PREFIX)/bin
	install w $(PREFIX)/bin
	install $(SCRIPTDIR)/awk $(PREFIX)/bin
	install $(SCRIPTDIR)/bzip2 $(PREFIX)/bin
	install $(SCRIPTDIR)/cat $(PREFIX)/bin
	install $(SCRIPTDIR)/cpp $(PREFIX)/bin
	install $(SCRIPTDIR)/cut $(PREFIX)/bin
	install $(SCRIPTDIR)/dd $(PREFIX)/bin
	install $(SCRIPTDIR)/df $(PREFIX)/bin
	install $(SCRIPTDIR)/echo $(PREFIX)/bin
	install $(SCRIPTDIR)/file $(PREFIX)/bin
	install $(SCRIPTDIR)/find $(PREFIX)/bin
	install $(SCRIPTDIR)/git $(PREFIX)/bin
	install $(SCRIPTDIR)/gzip $(PREFIX)/bin
	install $(SCRIPTDIR)/head $(PREFIX)/bin
	install $(SCRIPTDIR)/ls $(PREFIX)/bin
	install $(SCRIPTDIR)/nmrPipe $(PREFIX)/bin
	install $(SCRIPTDIR)/pamscale $(PREFIX)/bin
	install $(SCRIPTDIR)/perl $(PREFIX)/bin
	install $(SCRIPTDIR)/pgmmorphconv $(PREFIX)/bin
	install $(SCRIPTDIR)/pnmtopng $(PREFIX)/bin
	install $(SCRIPTDIR)/sed $(PREFIX)/bin
	install $(SCRIPTDIR)/sfcat $(PREFIX)/bin
	install $(SCRIPTDIR)/sffft1 $(PREFIX)/bin
	install $(SCRIPTDIR)/sffft3 $(PREFIX)/bin
	install $(SCRIPTDIR)/sfgrey $(PREFIX)/bin
	install $(SCRIPTDIR)/sfimag $(PREFIX)/bin
	install $(SCRIPTDIR)/sflmostretch $(PREFIX)/bin
	install $(SCRIPTDIR)/sfmath $(PREFIX)/bin
	install $(SCRIPTDIR)/sfput $(PREFIX)/bin
	install $(SCRIPTDIR)/sfreal $(PREFIX)/bin
	install $(SCRIPTDIR)/sfreverse $(PREFIX)/bin
	install $(SCRIPTDIR)/sfsmooth $(PREFIX)/bin
	install $(SCRIPTDIR)/sfspike $(PREFIX)/bin
	install $(SCRIPTDIR)/sfspray $(PREFIX)/bin
	install $(SCRIPTDIR)/sfwindow $(PREFIX)/bin
	install $(SCRIPTDIR)/sh $(PREFIX)/bin
	install $(SCRIPTDIR)/tail $(PREFIX)/bin
	install $(SCRIPTDIR)/tee $(PREFIX)/bin
	install $(SCRIPTDIR)/tr $(PREFIX)/bin
	install $(SCRIPTDIR)/uniq $(PREFIX)/bin
	install $(SCRIPTDIR)/var2pipe $(PREFIX)/bin
	install $(SCRIPTDIR)/wc $(PREFIX)/bin
	install $(SCRIPTDIR)/xargs $(PREFIX)/bin
	install $(SCRIPTDIR)/xz $(PREFIX)/bin


test-clean-bash:
	-rm $(PSDIR)/*.outb

test-clean:
	-rm $(PSDIR)/*.out

test: test-bash test-pseudo

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
		dgsh-wrap /bin/cat $(PSDIR)/f1s \
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
	$(DGSHPATH)/bash --dgsh -c ' \
		grep -v -w match $(PSDIR)/F $(PSDIR)/ff \
		| diff \
		> $(PSDIR)/grep_diff.outb' 2>$(PSDIR)/grep_diff.errb \
	&& sleep 0.01 \
	|| diff $(PSDIR)/grep_diff.outb $(PSDIR)/grep_diff.success-bash \
	&& printf "grep | diff $S\n" \
	|| (printf "grep | diff $F\n" \
	&& exit 1)
	
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
	$(DGSHPATH)/bash --dgsh -c ' \
		grep -l -L -w -v match $(PSDIR)/ff $(PSDIR)/F \
		| {{ \
			diff & \
			comm & \
		}} > $(PSDIR)/grep_diff_comm.outb' \
		2>$(PSDIR)/grep_diff_comm.errb \
	&& sleep 0.01 \
	&& (diff $(PSDIR)/grep_diff_comm.outb \
		$(PSDIR)/grep_diff_comm.success1-bash >/dev/null \
	    || diff $(PSDIR)/grep_diff_comm.outb \
		$(PSDIR)/grep_diff_comm.success2-bash >/dev/null \
	    || diff $(PSDIR)/grep_diff_comm.outb \
		$(PSDIR)/grep_diff_comm.success3-bash >/dev/null \
	    || diff $(PSDIR)/grep_diff_comm.outb \
		$(PSDIR)/grep_diff_comm.success4-bash >/dev/null) \
	&& printf "grep | {{ diff & comm & }} $S\n" \
	|| (printf "grep | {{ diff & comm & }} $F\n" \
	&& exit 1)
	
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
	$(DGSHPATH)/bash --dgsh $(XDIR)/dir.sh \
		>$(PSDIR)/dir.outb 2>$(PSDIR)/dir.err \
	&& printf "dir.sh $S\n" \
	|| (printf "dir.sh $F\n" \
	&& exit 1)
	
	# ditto
	cat $(PSDIR)/data.csv | \
	$(DGSHPATH)/bash --dgsh $(XDIR)/reorder-columns.sh \
		>$(PSDIR)/reorder-columns.outb 2>$(PSDIR)/reorder-columns.err \
	&& printf "reorder-columns.sh $S\n" \
	|| (printf "reorder-columns.sh $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh $(XDIR)/manage-results.sh $(PSDIR) \
		>$(PSDIR)/manage-results.outb 2>$(PSDIR)/manage-results.err \
	&& printf "manage-results.sh $S\n" \
	|| (printf "manage-results.sh $F\n" \
	&& exit 1)
	
	# ditto
	cat Readme.md | \
	$(DGSHPATH)/bash --dgsh $(XDIR)/compress-compare.sh \
		>$(PSDIR)/compress-compare.outb 2>$(PSDIR)/compress-compare.err \
	&& printf "compress-compare.sh $S\n" \
	|| (printf "compress-compare.sh $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh $(XDIR)/commit-stats.sh \
		>$(PSDIR)/commit-stats.outb 2>$(PSDIR)/commit-stats.err \
	&& printf "commit-stats.sh $S\n" \
	|| (printf "commit-stats.sh $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh $(XDIR)/duplicate-files.sh coreutils \
		>$(PSDIR)/duplicate-files.outb \
		2>$(PSDIR)/duplicate-files.err \
	&& printf "duplicate-files.sh $S\n" \
	|| (printf "duplicate-files.sh $F\n" \
	&& exit 1)
	
	# ditto
	cat Readme.md | \
	$(DGSHPATH)/bash --dgsh $(XDIR)/spell-highlight.sh \
		>$(PSDIR)/spell-highlight.outb 2>$(PSDIR)/spell-highlight.err \
	&& printf "spell-highlight.sh $S\n" \
	|| (printf "spell-highlight.sh $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh $(XDIR)/static-functions.sh coreutils \
		>$(PSDIR)/static-functions.outb 2>$(PSDIR)/static-functions.err \
	&& printf "static-functions.sh $S\n" \
	|| (printf "static-functions.sh $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh $(XDIR)/word-properties.sh Readme.md \
		>$(PSDIR)/word-properties.outb 2>$(PSDIR)/word-properties.err \
	&& printf "word-properties.sh $S\n" \
	|| (printf "word-properties.sh $F\n"	\
	&& exit 1)
	
	# ditto
	-rm -rf grep2 grep-p100
	cp -R grep grep2 && \
	cp -R grep2 grep-p100 && \
	cd grep-p100 && git checkout HEAD~100 >/dev/null 2>/dev/null && \
	cd ../grep2 && git checkout master >/dev/null 2>/dev/null && cd ..
	$(DGSHPATH)/bash --dgsh $(XDIR)/map-hierarchy.sh grep2 grep-p100 grepp \
		>$(PSDIR)/map-hierarchy.outb 2>$(PSDIR)/map-hierarchy.err \
	&& printf "map-hierarchy.sh $S\n" \
	|| (printf "map-hierarchy.sh $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh $(XDIR)/text-properties.sh Readme.md \
		>$(PSDIR)/text-properties.outb 2>$(PSDIR)/text-properties.err \
	&& printf "text-properties.sh $S\n" \
	|| (printf "text-properties.sh $F\n" \
	&& exit 1)
	
	# ditto
	cd coreutils && $(DGSHPATH)/bash --dgsh ../$(XDIR)/committer-plot.sh \
		>../$(PSDIR)/committer-plot.outb \
		2>../$(PSDIR)/committer-plot.err \
	&& printf "committer-plot.sh $S\n" \
	|| (printf "committer-plot.sh $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh $(XDIR)/code-metrics.sh coreutils \
		>$(PSDIR)/code-metrics.outb 2>$(PSDIR)/code-metrics.err \
	&& printf "code-metrics.sh $S\n" \
	|| (printf "code-metrics.sh $F\n" \
	&& exit 1)
	
	# ditto
	cat $(PSDIR)/access.log | \
	$(DGSHPATH)/bash --dgsh $(XDIR)/web-log-report.sh \
		>$(PSDIR)/web-log-report.outb 2>$(PSDIR)/web-log-report.err \
	&& printf "web-log-report.sh $S\n" \
	|| (printf "web-log-report.sh $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh $(XDIR)/fft-block8.sh $(PSDIR)/fft-input.dat \
		>$(PSDIR)/fft-block8.outb 2>$(PSDIR)/fft-block8.err \
	&& printf "fft-block8.sh $S\n" \
	|| (printf "fft-block8.sh $F\n" \
	&& exit 1)
	
	# ditto
	$(DGSHPATH)/bash --dgsh $(XDIR)/ft2d.sh \
		>$(PSDIR)/ft2d.outb 2>$(PSDIR)/ft2d.err \
	&& printf "ft2d.sh $S\n" \
	|| (printf "ft2d.sh $F\n" \
	&& exit 1)
	
	# ditto
	# Requires file fid in $(PSDIR). See $(XDIR)/NMRPipe.sh
	$(DGSHPATH)/bash --dgsh $(XDIR)/NMRPipe.sh $(PSDIR)/fid \
		>$(PSDIR)/NMRPipe.outb 2>$(PSDIR)/NMRPipe.err \
	&& printf "NMRPipe.sh $S\n" \
	|| (printf "NMRPipe.sh $F\n" \
	&& exit 1)
	
test-pseudo: test-clean
	printf "$B\nPseudo shell tests:${EC}\n"
	
	cd $(PSDIR) && \
	python2 simple-shell.py secho_paste.dgsh secho_paste.out \
		2>$(PSDIR)/secho_paste.err \
	&& sleep 0.01 \
	&& diff secho_paste.out secho_paste.success \
	&& printf "secho | paste $S\n" \
	|| (printf "secho | paste $F\n" \
	&& exit 1)

	cd $(PSDIR) && \
	python2 simple-shell.py wrap-cat_comm_sort.dgsh wrap-cat_comm_sort.out \
		2>$(PSDIR)/wrap-cat_comm_sort.err \
	&& sleep 0.01 \
	&& diff wrap-cat_comm_sort.out wrap-cat_comm_sort.success \
	&& printf "dgsh-wrap cat | comm | sort $S\n" \
	|| (printf "dgsh-wrap cat | comm | sort $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py comm_sort.dgsh comm_sort.out \
		2>$(PSDIR)/comm_sort.err \
	&& diff comm_sort.out comm_sort.success \
	&& printf "comm | sort $S\n" \
	|| (printf "comm | sort $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py comm_paste.dgsh comm_paste.out \
		2>$(PSDIR)/comm_paste.err \
	&& sleep 0.01 \
	&& diff comm_paste.out comm_paste.success \
	&& printf "comm | paste $S\n" \
	|| (printf "comm | paste $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py join_sort.dgsh join_sort.out \
		2>$(PSDIR)/join_sort.err \
	&& sleep 0.01 \
	&& diff join_sort.out join_sort.success \
	&& printf "join | sort $S\n" \
	|| (printf "join | sort $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py paste_diff.dgsh paste_diff.out \
		2>$(PSDIR)/paste_diff.err \
	&& sleep 0.01 \
	&& diff paste_diff.out paste_diff.success \
	&& printf "paste | diff $S\n" \
	|| (printf "paste | diff $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py grep_diff.dgsh grep_diff.out \
		2>$(PSDIR)/grep_diff.err \
	&& sleep 0.01 \
	&& diff grep_diff.out grep_diff.success \
	&& printf "grep | diff $S\n" \
	|| (printf "grep | diff $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py grep_comm.dgsh grep_comm.out \
		2>$(PSDIR)/grep_comm.err \
	&& diff grep_comm.out grep_comm.success \
	&& printf "grep | comm $S\n" \
	|| (printf "grep | comm $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py join_sort_diff.dgsh join_sort_diff.out \
		2>$(PSDIR)/join_sort_diff.err \
	&& sleep 0.01 \
	&& diff join_sort_diff.out join_sort_diff.success \
	&& printf "join | sort | diff $S\n" \
	|| (printf "join | sort | diff $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py dir-plain.dgsh dir-plain.out \
		2>$(PSDIR)/dir-plain.err \
	&& sleep 0.01 \
	&& printf "ls -n | {{ awk & awk & }} $S\n" \
	|| (printf "ls -n | {{ awk & awk & }} $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py secho_secho_fgrep.dgsh secho_secho_fgrep.out \
		2>$(PSDIR)/secho_secho_fgrep.err \
	&& sleep 0.01 \
	&& diff $(PSDIR)/secho_secho_fgrep.out \
		$(PSDIR)/secho_secho_fgrep.success \
	&& printf "{{ secho & secho & }} | grep -F $S\n" \
	|| (printf "{{ secho & secho & }} | grep -F $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py tee-copy_diff_comm.dgsh tee-copy_diff_comm.out \
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
	python2 simple-shell.py grep_diff_comm.dgsh grep_diff_comm.out \
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
	python2 simple-shell.py sort_sort_comm.dgsh sort_sort_comm.out \
		2>$(PSDIR)/sort_sort_comm.err \
	&& sleep 0.01 \
	&& $(STD)/diff sort_sort_comm.out sort_sort_comm.success \
	&& printf "{{ sort & sort & }} | comm $S\n" \
	|| (printf "{{ sort & sort & }} | comm $F\n" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py comm_paste_join_diff.dgsh \
		comm_paste_join_diff.out 2>$(PSDIR)/comm_paste_join_diff.err \
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
		sort_sort_comm_paste_join_diff.dgsh \
		sort_sort_comm_paste_join_diff.out \
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
	$(MAKE) -C coreutils clean
	$(MAKE) -C diffutils clean
	$(MAKE) -C grep clean
