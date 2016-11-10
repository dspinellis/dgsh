INSTPREFIX=/usr/local/sgsh
SGSHPATH=/usr/local/sgsh/bin

SGSHDIR=..
PSDIR=../simple-shell
XDIR=../example
# TOOLDIR
TD=unix-sgsh-tools
STD=/usr/bin
SCRIPTDIR=tool_scripts

CFLAGS+=-Wall
PATHS= -I.. -L..
LIBS= -lsgsh_negotiate

# Color
GR=\033[0;32m	# Green
R=\033[0;31m	# Red
B=\033[0;34m	# Blue
EC=\033[0m	# End color
S=${GR}successful${EC}
F=${R}failed${EC}

# If not cloned repo with --recursive use this target to
# clone the submodule repos
get-submodules:
	cd .. && git submodule update --init --recursive \
		$(TD)/bash \
		$(TD)/coreutils \
		$(TD)/diffutils \
		$(TD)/grep

configure:
	# To build with DEBUG config do ./configure CFLAGS=-DDEBUG ...
	cd bash && ./configure --prefix=$(INSTPREFIX)
	cd coreutils && ./bootstrap && ./configure --prefix=$(INSTPREFIX)
	cd diffutils && ./bootstrap && ./configure --prefix=$(INSTPREFIX)
	cd grep && ./bootstrap && ./configure --prefix=$(INSTPREFIX)

make:
	$(MAKE) -C bash
	$(MAKE) -C coreutils
	$(MAKE) -C diffutils
	$(MAKE) -C grep
	$(CC) $(CFLAGS) $(PATHS) simple_echo.c -o secho $(LIBS)
	$(CC) $(CFLAGS) $(PATHS) pecho.c -o pecho $(LIBS)

install:
	$(MAKE) -C bash install
	$(MAKE) -C coreutils install
	$(MAKE) -C diffutils install
	$(MAKE) -C grep install
	# Install last to overwrite standard tools of coreutils
	install $(SGSHDIR)/sgsh-tee $(INSTPREFIX)/bin/tee
	install $(SGSHDIR)/sgsh-tee $(INSTPREFIX)/bin/cat
	install secho $(INSTPREFIX)/bin
	install pecho $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/awk $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/bzip2 $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/cpp $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/cut $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/df $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/echo $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/file $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/find $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/git $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/gzip $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/head $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/ls $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/nmrPipe $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/pamscale $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/perl $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/pgmmorphconv $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/pnmtopng $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sed $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfcat $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sffft1 $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sffft3 $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfgrey $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfimag $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sflmostretch $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfmath $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfput $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfreal $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfreverse $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfsmooth $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfspike $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfspray $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sfwindow $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/sh $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/tail $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/tr $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/uniq $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/var2pipe $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/wc $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/xargs $(INSTPREFIX)/bin
	install $(SCRIPTDIR)/xz $(INSTPREFIX)/bin


test-clean-bash:
	-rm $(PSDIR)/*.outb

test-clean:
	-rm $(PSDIR)/*.out

test: test-bash test-pseudo

path:
	export PATH
	echo $(PATH)

test-bash: test-clean
	echo "$B\nBash tests:${EC}"
	
	$(SGSHPATH)/bash --sgsh -c ' \
		secho hello \
		| paste $(PSDIR)/world \
		> $(PSDIR)/secho_paste.outb' 2>$(PSDIR)/secho_paste.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/secho_paste.outb $(PSDIR)/secho_paste.success \
	&& echo "secho | paste $S" \
	|| (echo "secho | paste $F" \
	&& exit 1)

	$(SGSHPATH)/bash --sgsh -c ' \
		sgsh-wrap /bin/cat $(PSDIR)/f1s \
		| comm $(PSDIR)/f2s \
		| sort \
		| wc -l \
		> $(PSDIR)/wrap-cat_comm_sort.outb' \
		2>$(PSDIR)/wrap-cat_comm_sort.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/wrap-cat_comm_sort.outb \
		$(PSDIR)/wrap-cat_comm_sort.success \
	&& echo "sgsh-wrap cat | comm | sort $S" \
	|| (echo "sgsh-wrap cat | comm | sort $F" \
	&& exit 1)
	
	$(SGSHPATH)/bash --sgsh -c ' \
		comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		| wc -l > $(PSDIR)/comm_sort.outb' 2>$(PSDIR)/comm_sort.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/comm_sort.outb $(PSDIR)/comm_sort.success \
	&& echo "comm | sort $S" \
	|| (echo "comm | sort $F" \
	&& exit 1)
	
	$(SGSHPATH)/bash --sgsh -c ' \
		comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| paste \
		> $(PSDIR)/comm_paste.outb' 2>$(PSDIR)/comm_paste.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/comm_paste.outb \
		$(PSDIR)/comm_paste.success \
	&& echo "comm | paste $S" \
	|| (echo "comm | paste $F" \
	&& exit 1)
	
	$(SGSHPATH)/bash --sgsh -c ' \
		join $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		> $(PSDIR)/join_sort.outb' 2>$(PSDIR)/join_sort.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/join_sort.outb \
		$(PSDIR)/join_sort.success \
	&& echo "join | sort $S" \
	|| (echo "join | sort $F" \
	&& exit 1)
	
	# || instead of && in the following test because diff's exit 
	# status becomes the command's exit status
	$(SGSHPATH)/bash --sgsh -c ' \
		paste $(PSDIR)/f1s $(PSDIR)/f2s \
		| diff $(PSDIR)/f1s \
		> $(PSDIR)/paste_diff.outb' 2>$(PSDIR)/paste_diff.errb \
	&& sleep 0.01 \
	|| diff $(PSDIR)/paste_diff.outb $(PSDIR)/paste_diff.success \
	&& echo "paste | diff $S" \
	|| (echo "paste | diff $F" \
	&& exit 1)
	
	#ditto
	$(SGSHPATH)/bash --sgsh -c ' \
		grep -v -w match $(PSDIR)/F $(PSDIR)/ff \
		| diff \
		> $(PSDIR)/grep_diff.outb' 2>$(PSDIR)/grep_diff.errb \
	&& sleep 0.01 \
	|| diff $(PSDIR)/grep_diff.outb $(PSDIR)/grep_diff.success-bash \
	&& echo "grep | diff $S" \
	|| (echo "grep | diff $F" \
	&& exit 1)
	
	$(SGSHPATH)/bash --sgsh -c ' \
		grep -l -L match $(PSDIR)/ff $(PSDIR)/F \
		| comm \
		> $(PSDIR)/grep_comm.outb' 2>$(PSDIR)/grep_comm.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/grep_comm.outb $(PSDIR)/grep_comm.success-bash \
	&& echo "grep | comm $S" \
	|| (echo "grep | comm $F" \
	&& exit 1)
	
	# ditto
	$(SGSHPATH)/bash --sgsh -c ' \
		join $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		| diff $(PSDIR)/f3s \
		> $(PSDIR)/join_sort_diff.outb' 2>$(PSDIR)/join_sort_diff.errb \
	&& sleep 0.01 \
	|| diff $(PSDIR)/join_sort_diff.outb \
		$(PSDIR)/join_sort_diff.success \
	&& echo "join | sort | diff $S" \
	|| (echo "join | sort | diff $F" \
	&& exit 1)
	
	# No diff, just check execution exit status
	$(SGSHPATH)/bash --sgsh $(PSDIR)/dir-plain.sh \
		>$(PSDIR)/dir-plain.outb 2>$(PSDIR)/dir-plain.errb \
	&& echo "ls -n | {{ awk & awk & }} $S" \
	|| (echo "ls -n | {{ awk & awk & }} $F" \
	&& exit 1)
	
	# ditto
	# `date`: Check that command substitution
	# does not mess pipe substitution
	$(SGSHPATH)/bash --sgsh -c ' \
		{{ \
			secho match & \
			secho "not `date`" & \
		}} \
		| grep -F -h match \
	> $(PSDIR)/secho_secho_fgrep.outb' 2>$(PSDIR)/secho_secho_fgrep.errb \
	&& sleep 0.01 \
	&& diff $(PSDIR)/secho_secho_fgrep.outb \
		$(PSDIR)/secho_secho_fgrep.success \
	&& echo "{{ secho & secho & }} | grep -F $S" \
	|| (echo "{{ secho & secho & }} | grep -F $F" \
	&& exit 1)
	
	$(SGSHPATH)/bash --sgsh -c ' \
		tee -i $(PSDIR)/hello \
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
	&& echo "tee | {{ diff & comm & }} $S" \
	|| (echo "tee | {{ diff & comm & }} $F" \
	&& exit 1)
	
	$(SGSHPATH)/bash --sgsh -c ' \
		tee -i $(PSDIR)/hello -s \
		| {{ \
			diff $(PSDIR)/world & \
			comm $(PSDIR)/hello & \
		}} > $(PSDIR)/tee-scatter_diff_comm.outb' \
		2>$(PSDIR)/tee-scatter_diff_comm.errb \
	&& sleep 0.01 \
	&& (diff $(PSDIR)/tee-scatter_diff_comm.outb \
			$(PSDIR)/tee-scatter_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-scatter_diff_comm.outb \
			$(PSDIR)/tee-scatter_diff_comm.success2 >/dev/null) \
	&& echo "tee -s | {{ diff & comm & }} $S" \
	|| (echo "tee -s | {{ diff & comm & }} $F" \
	&& exit 1)
	
	# ditto
	$(SGSHPATH)/bash --sgsh -c ' \
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
		$(PSDIR)/grep_diff_comm.success2-bash >/dev/null) \
	&& echo "grep | {{ diff & comm & }} $S" \
	|| (echo "grep | {{ diff & comm & }} $F" \
	&& exit 1)
	
	$(SGSHPATH)/bash --sgsh -c ' \
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
	&& echo "comm | {{ paste & join & diff & }} $S" \
	|| (echo "comm | {{ paste & join & diff & }} $F" \
	&& exit 1)
	
	$(SGSHPATH)/bash --sgsh -c ' \
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
	&& echo "{{ sort & sort & }} | comm $S" \
	|| (echo "{{ sort & sort & }} | comm $F" \
	&& exit 1)
	
	$(SGSHPATH)/bash --sgsh -c ' \
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
	&& echo "{{ sort & sort & }} | comm | {{ paste & join & diff & }} $S" \
	|| (echo "{{ sort & sort & }} | comm | {{ paste & join & diff & }} $F" \
	&& exit 1)
	
	echo " $BPaper examples under ../example:${EC}"

	# No diff, just check execution exit status
	$(SGSHPATH)/bash --sgsh $(XDIR)/dir.sh \
		>$(PSDIR)/dir.outb 2>$(PSDIR)/dir.err \
	&& echo "dir.sh $S" \
	|| (echo "dir.sh $F" \
	&& exit 1)
	
	# ditto
	cat $(PSDIR)/data.csv | \
	$(SGSHPATH)/bash --sgsh $(XDIR)/reorder-columns.sh \
		>$(PSDIR)/reorder-columns.outb 2>$(PSDIR)/reorder-columns.err \
	&& echo "reorder-columns.sh $S" \
	|| (echo "reorder-columns.sh $F" \
	&& exit 1)
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/manage-results.sh $(PSDIR) \
		>$(PSDIR)/manage-results.outb 2>$(PSDIR)/manage-results.err \
	&& echo "manage-results.sh $S" \
	|| (echo "manage-results.sh $F" \
	&& exit 1)
	
	# ditto
	cat Readme.md | \
	$(SGSHPATH)/bash --sgsh $(XDIR)/compress-compare.sh \
		>$(PSDIR)/compress-compare.outb 2>$(PSDIR)/compress-compare.err \
	&& echo "compress-compare.sh $S" \
	|| (echo "compress-compare.sh $F" \
	&& exit 1)
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/commit-stats.sh Readme.md \
		>$(PSDIR)/commit-stats.outb 2>$(PSDIR)/commit-stats.err \
	&& echo "commit-stats.sh $S" \
	|| (echo "commit-stats.sh $F" \
	&& exit 1)
	
	# ditto
	cd coreutils && \
	$(SGSHPATH)/bash --sgsh ../$(XDIR)/duplicate-files.sh \
		>../$(PSDIR)/duplicate-files.outb \
		2>../$(PSDIR)/duplicate-files.err \
	&& echo "duplicate-files.sh $S" \
	|| (echo "duplicate-files.sh $F" \
	&& exit 1)
	
	# ditto
	cat Readme.md | \
	$(SGSHPATH)/bash --sgsh $(XDIR)/spell-highlight.sh \
		>$(PSDIR)/spell-highlight.outb 2>$(PSDIR)/spell-highlight.err \
	&& echo "spell-highlight.sh $S" \
	|| (echo "spell-highlight.sh $F" \
	&& exit 1)
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/static-functions.sh coreutils \
		>$(PSDIR)/static-functions.outb 2>$(PSDIR)/static-functions.err \
	&& echo "static-functions.sh $S" \
	|| (echo "static-functions.sh $F" \
	&& exit 1)
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/word-properties.sh Readme.md \
		>$(PSDIR)/word-properties.outb 2>$(PSDIR)/word-properties.err \
	&& echo "word-properties.sh $S" \
	|| (echo "word-properties.sh $F"	\
	&& exit 1)
	
	# ditto
	-rm -rf grep-p100
	cp -R grep grep-p100 && \
	cd grep-p100 && git checkout HEAD~100 >/dev/null 2>/dev/null && \
	cd ../grep && git checkout master >/dev/null 2>/dev/null && cd ..
	$(SGSHPATH)/bash --sgsh $(XDIR)/map-hierarchy.sh grep grep-p100 grepp \
		>$(PSDIR)/map-hierarchy.outb 2>$(PSDIR)/map-hierarchy.err \
	&& echo "map-hierarchy.sh $S" \
	|| (echo "map-hierarchy.sh $F" \
	&& exit 1)
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/text-properties.sh Readme.md \
		>$(PSDIR)/text-properties.outb 2>$(PSDIR)/text-properties.err \
	&& echo "text-properties.sh $S" \
	|| (echo "text-properties.sh $F" \
	&& exit 1)
	
	# ditto
	cd coreutils && $(SGSHPATH)/bash --sgsh ../$(XDIR)/committer-plot.sh \
		>../$(PSDIR)/committer-plot.outb \
		2>../$(PSDIR)/committer-plot.err \
	&& echo "committer-plot.sh $S" \
	|| (echo "committer-plot.sh $F" \
	&& exit 1)
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/code-metrics.sh \
		>$(PSDIR)/code-metrics.outb 2>$(PSDIR)/code-metrics.err \
	&& echo "code-metrics.sh $S" \
	|| (echo "code-metrics.sh $F" \
	&& exit 1)
	
	# ditto
	cat $(PSDIR)/access.log | \
	$(SGSHPATH)/bash --sgsh $(XDIR)/web-log-report.sh \
		>$(PSDIR)/web-log-report.outb 2>$(PSDIR)/web-log-report.err \
	&& echo "web-log-report.sh $S" \
	|| (echo "web-log-report.sh $F" \
	&& exit 1)
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/ft2d.sh \
		>$(PSDIR)/ft2d.outb 2>$(PSDIR)/ft2d.err \
	&& echo "ft2d.sh $S" \
	|| (echo "ft2d.sh $F" \
	&& exit 1)
	
	# ditto
	# Requires file fid in $(PSDIR). See $(XDIR)/NMRPipe.sh
	$(SGSHPATH)/bash --sgsh $(XDIR)/NMRPipe.sh $(PSDIR)/fid \
		>$(PSDIR)/NMRPipe.outb 2>$(PSDIR)/NMRPipe.err \
	&& echo "NMRPipe.sh $S" \
	|| (echo "NMRPipe.sh $F" \
	&& exit 1)
	
	mv *.dot coreutils/*.dot $(XDIR)

test-pseudo: test-clean
	echo "$B\nPseudo shell tests:${EC}"
	
	cd $(PSDIR) && \
	python2 simple-shell.py secho_paste.sgsh secho_paste.out \
		2>$(PSDIR)/secho_paste.err \
	&& sleep 0.01 \
	&& diff secho_paste.out secho_paste.success \
	&& echo "secho | paste $S" \
	|| (echo "secho | paste $F" \
	&& exit 1)

	cd $(PSDIR) && \
	python2 simple-shell.py wrap-cat_comm_sort.sgsh wrap-cat_comm_sort.out \
		2>$(PSDIR)/wrap-cat_comm_sort.err \
	&& sleep 0.01 \
	&& diff wrap-cat_comm_sort.out wrap-cat_comm_sort.success \
	&& echo "sgsh-wrap cat | comm | sort $S" \
	|| (echo "sgsh-wrap cat | comm | sort $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py comm_sort.sgsh comm_sort.out \
		2>$(PSDIR)/comm_sort.err \
	&& diff comm_sort.out comm_sort.success \
	&& echo "comm | sort $S" \
	|| (echo "comm | sort $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py comm_paste.sgsh comm_paste.out \
		2>$(PSDIR)/comm_paste.err \
	&& sleep 0.01 \
	&& diff comm_paste.out comm_paste.success \
	&& echo "comm | paste $S" \
	|| (echo "comm | paste $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py join_sort.sgsh join_sort.out \
		2>$(PSDIR)/join_sort.err \
	&& sleep 0.01 \
	&& diff join_sort.out join_sort.success \
	&& echo "join | sort $S" \
	|| (echo "join | sort $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py paste_diff.sgsh paste_diff.out \
		2>$(PSDIR)/paste_diff.err \
	&& sleep 0.01 \
	&& diff paste_diff.out paste_diff.success \
	&& echo "paste | diff $S" \
	|| (echo "paste | diff $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py grep_diff.sgsh grep_diff.out \
		2>$(PSDIR)/grep_diff.err \
	&& sleep 0.01 \
	&& diff grep_diff.out grep_diff.success \
	&& echo "grep | diff $S" \
	|| (echo "grep | diff $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py grep_comm.sgsh grep_comm.out \
		2>$(PSDIR)/grep_comm.err \
	&& diff grep_comm.out grep_comm.success \
	&& echo "grep | comm $S" \
	|| (echo "grep | comm $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py join_sort_diff.sgsh join_sort_diff.out \
		2>$(PSDIR)/join_sort_diff.err \
	&& sleep 0.01 \
	&& diff join_sort_diff.out join_sort_diff.success \
	&& echo "join | sort | diff $S" \
	|| (echo "join | sort | diff $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py dir-plain.sgsh dir-plain.out \
		2>$(PSDIR)/dir-plain.err \
	&& sleep 0.01 \
	&& echo "ls -n | {{ awk & awk & }} $S" \
	|| (echo "ls -n | {{ awk & awk & }} $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py secho_secho_fgrep.sgsh secho_secho_fgrep.out \
		2>$(PSDIR)/secho_secho_fgrep.err \
	&& sleep 0.01 \
	&& diff $(PSDIR)/secho_secho_fgrep.out \
		$(PSDIR)/secho_secho_fgrep.success \
	&& echo "{{ secho & secho & }} | grep -F $S" \
	|| (echo "{{ secho & secho & }} | grep -F $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py tee-copy_diff_comm.sgsh tee-copy_diff_comm.out \
		2>$(PSDIR)/tee-copy_diff_comm.err \
	&& sleep 0.01 \
	&& (diff $(PSDIR)/tee-copy_diff_comm.out \
			$(PSDIR)/tee-copy_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-copy_diff_comm.out \
			$(PSDIR)/tee-copy_diff_comm.success2 >/dev/null) \
	&& echo "tee | {{ diff & comm & }} $S" \
	|| (echo "tee | {{ diff & comm & }} $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py tee-scatter_diff_comm.sgsh tee-scatter_diff_comm.out \
		2>$(PSDIR)/tee-scatter_diff_comm.err \
	&& sleep 0.01 \
	&& (diff $(PSDIR)/tee-scatter_diff_comm.out \
			$(PSDIR)/tee-scatter_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-scatter_diff_comm.out \
			$(PSDIR)/tee-scatter_diff_comm.success2 >/dev/null) \
	&& echo "tee -s | {{ diff & comm & }} $S" \
	|| (echo "tee -s | {{ diff & comm & }} $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py grep_diff_comm.sgsh grep_diff_comm.out \
		2>$(PSDIR)/grep_diff_comm.err \
	&& sleep 0.01 \
	&& ($(STD)/diff grep_diff_comm.out grep_diff_comm.success1 >/dev/null \
	    || $(STD)/diff grep_diff_comm.out grep_diff_comm.success2 >/dev/null) \
	&& echo "grep | {{ diff & comm & }} $S" \
	|| (echo "grep | {{ diff & comm & }} $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py sort_sort_comm.sgsh sort_sort_comm.out \
		2>$(PSDIR)/sort_sort_comm.err \
	&& sleep 0.01 \
	&& $(STD)/diff sort_sort_comm.out sort_sort_comm.success \
	&& echo "{{ sort & sort & }} | comm $S" \
	|| (echo "{{ sort & sort & }} | comm $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py comm_paste_join_diff.sgsh \
		comm_paste_join_diff.out 2>$(PSDIR)/comm_paste_join_diff.err \
	&& sleep 0.01 \
	&& (diff comm_paste_join_diff.out \
		comm_paste_join_diff.success1 >/dev/null \
	|| diff comm_paste_join_diff.out \
		comm_paste_join_diff.success2 > /dev/null) \
	&& echo "comm | {{ paste & join & diff & }} $S" \
	|| (echo "comm | {{ paste & join & diff & }} $F" \
	&& exit 1)
	
	cd $(PSDIR) && \
	python2 simple-shell.py \
		sort_sort_comm_paste_join_diff.sgsh \
		sort_sort_comm_paste_join_diff.out \
		2>$(PSDIR)/sort_sort_comm_paste_join_diff.err \
	&& sleep 0.01 \
	&& (diff sort_sort_comm_paste_join_diff.out \
		sort_sort_comm_paste_join_diff.success1 >/dev/null \
	|| diff sort_sort_comm_paste_join_diff.out \
		sort_sort_comm_paste_join_diff.success2 >/dev/null) \
	&& echo "{{ sort & sort & }} | comm | {{ paste & join & diff & }} $S" \
	|| (echo "{{ sort & sort & }} | comm | {{ paste & join & diff & }} $F" \
	&& exit 1)

clean:
	$(MAKE) -C bash clean
	$(MAKE) -C coreutils clean
	$(MAKE) -C diffutils clean
	$(MAKE) -C grep clean

