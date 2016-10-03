INSTPREFIX=/usr/local/sgsh
SGSHPATH=/usr/local/sgsh/bin

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
		$(TD)/grep \
		$(TD)/parallel \
		$(TD)/sed \
		$(TD)/tar

configure:
	cd bash && ./configure --prefix=$(INSTPREFIX)
	cd coreutils && ./bootstrap && ./configure --prefix=$(INSTPREFIX)
	cd diffutils && ./bootstrap && ./configure --prefix=$(INSTPREFIX)
	#cd gawk && ./bootstrap.sh && ./configure --prefix=`pwd`/..
	cd grep && ./bootstrap && ./configure --prefix=$(INSTPREFIX)
	#cd parallel && ./configure --prefix=
	#cd sed && ./bootstrap && ./configure --prefix=`pwd`/..
	#cd tar && ./bootstrap && ./configure --prefix=`pwd`/..

make:
	cd bash && make
	cd coreutils && make
	cd diffutils && make
	#cd gawk && make
	cd grep && make
	#cd parallel && make
	#cd sed && make
	#cd tar && make
	$(CC) $(CFLAGS) $(PATHS) simple_echo.c -o secho $(LIBS)
	$(CC) $(CFLAGS) $(PATHS) pecho.c -o pecho $(LIBS)

install:
	cd bash && make install
	cd coreutils && make install
	cd diffutils && make install
	#cd gawk && make install
	cd grep && make install
	#cd parallel && make install
	#cd sed && make install
	#cd tar && make install
	cp secho $(INSTPREFIX)/bin
	cp pecho $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/awk $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/bzip2 $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/cat $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/cut $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/df $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/echo $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/file $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/find $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/git $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/gzip $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/head $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/ls $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/pamscale $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/perl $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/pgmmorphconv $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/pnmtopng $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/sed $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/sh $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/tail $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/tr $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/uniq $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/wc $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/xargs $(INSTPREFIX)/bin
	cp $(SCRIPTDIR)/xz $(INSTPREFIX)/bin


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
		| paste - $(PSDIR)/world \
		> $(PSDIR)/secho_paste.outb' 2>/dev/null \
	&& diff $(PSDIR)/secho_paste.outb $(PSDIR)/secho_paste.success \
	&& echo 'secho | paste $S' \
	|| echo 'secho | paste $F'

	$(SGSHPATH)/bash --sgsh -c ' \
		sgsh-wrap /bin/cat $(PSDIR)/f1s \
		| comm - $(PSDIR)/f2s \
		| sort \
		> $(PSDIR)/wrap-cat_comm_sort.outb' 2>/dev/null \
	&& diff $(PSDIR)/wrap-cat_comm_sort.outb \
		$(PSDIR)/wrap-cat_comm_sort.success \
	&& echo 'sgsh-wrap cat | comm | sort $S' \
	|| echo 'sgsh-wrap cat | comm | sort $F'
	
	$(SGSHPATH)/bash --sgsh -c ' \
		comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort > $(PSDIR)/comm_sort.outb' 2>/dev/null \
	&& diff $(PSDIR)/comm_sort.outb $(PSDIR)/comm_sort.success \
	&& echo 'comm | sort $S' \
	|| echo 'comm | sort $F'
	
	$(SGSHPATH)/bash --sgsh -c ' \
		comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| paste - - - \
		> $(PSDIR)/comm_paste.outb' 2>/dev/null \
	&& diff $(PSDIR)/comm_paste.outb \
		$(PSDIR)/comm_paste.success \
	&& echo 'comm | paste $S' \
	|| echo 'comm | paste $F'
	
	$(SGSHPATH)/bash --sgsh -c ' \
		join $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		> $(PSDIR)/join_sort.outb' 2>/dev/null \
	&& diff $(PSDIR)/join_sort.outb \
		$(PSDIR)/join_sort.success \
	&& echo 'join | sort $S' \
	|| echo 'join | sort $F'
	
	# || instead of && in the following test because diff's exit 
	# status becomes the command's exit status
	$(SGSHPATH)/bash --sgsh -c ' \
		paste $(PSDIR)/f1s $(PSDIR)/f2s \
		| diff $(PSDIR)/f1s - \
		> $(PSDIR)/paste_diff.outb' 2>/dev/null \
	|| diff $(PSDIR)/paste_diff.outb $(PSDIR)/paste_diff.success \
	&& echo 'paste | diff $S' \
	|| echo 'paste | diff $F'
	
	#ditto
	$(SGSHPATH)/bash --sgsh -c ' \
		grep -v -w match $(PSDIR)/f $(PSDIR)/F \
		| diff - - \
		> $(PSDIR)/grep_diff.outb' 2>/dev/null \
	|| diff $(PSDIR)/grep_diff.outb $(PSDIR)/grep_diff.success-bash \
	&& echo 'grep | diff $S' \
	|| echo 'grep | diff $F'
	
	$(SGSHPATH)/bash --sgsh -c ' \
		grep -l -L match $(PSDIR)/f $(PSDIR)/F \
		| comm - - \
		> $(PSDIR)/grep_comm.outb' 2>/dev/null \
	|| diff $(PSDIR)/grep_comm.outb $(PSDIR)/grep_comm.success \
	&& echo 'grep | comm $S' \
	|| echo 'grep | comm $F'
	
	# ditto
	$(SGSHPATH)/bash --sgsh -c ' \
		join $(PSDIR)/f1s $(PSDIR)/f2s \
		| sort \
		| diff $(PSDIR)/f3s - \
		> $(PSDIR)/join_sort_diff.outb' 2>/dev/null \
	|| diff $(PSDIR)/join_sort_diff.outb \
		$(PSDIR)/join_sort_diff.success \
	&& echo 'join | sort | diff $S' \
	|| echo 'join | sort | diff $F'
	
	# No diff, just check execution exit status
	$(SGSHPATH)/bash --sgsh $(PSDIR)/dir-plain.sh \
		>$(PSDIR)/dir-plain.outb 2>/dev/null \
	&& echo 'ls -n | {{ awk & awk & }} $S' \
	|| echo 'ls -n | {{ awk & awk & }} $F'
	
	# ditto
	# `date`: Check that command substitution
	# does not mess pipe substitution
	$(SGSHPATH)/bash --sgsh -c ' \
		{{ \
			secho match & \
			secho "not `date`" & \
		}} \
		| grep -F -h match - - \
	> $(PSDIR)/secho_secho_fgrep.outb' 2>/dev/null \
	&& diff $(PSDIR)/secho_secho_fgrep.outb \
		$(PSDIR)/secho_secho_fgrep.success \
	&& echo '{{ secho & secho & }} | grep -F $S' \
	|| echo '{{ secho & secho & }} | grep -F $F'
	
	$(SGSHPATH)/bash --sgsh -c ' \
		sgsh-tee -i $(PSDIR)/hello \
		| {{ \
			diff $(PSDIR)/world - & \
			comm $(PSDIR)/hello - & \
		}} > $(PSDIR)/tee-copy_diff_comm.outb' 2>/dev/null \
	&& (diff $(PSDIR)/tee-copy_diff_comm.outb \
			$(PSDIR)/tee-copy_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-copy_diff_comm.outb \
			$(PSDIR)/tee-copy_diff_comm.success2 >/dev/null) \
	&& echo 'sgsh-tee | {{ diff & comm & }} $S' \
	|| echo 'sgsh-tee | {{ diff & comm & }} $F'
	
	$(SGSHPATH)/bash --sgsh -c ' \
		sgsh-tee -i $(PSDIR)/hello -s \
		| {{ \
			diff $(PSDIR)/world - & \
			comm $(PSDIR)/hello - & \
		}} > $(PSDIR)/tee-scatter_diff_comm.outb' 2>/dev/null \
	&& (diff $(PSDIR)/tee-scatter_diff_comm.outb \
			$(PSDIR)/tee-scatter_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-scatter_diff_comm.outb \
			$(PSDIR)/tee-scatter_diff_comm.success2 >/dev/null) \
	&& echo 'sgsh-tee -s | {{ diff & comm & }} $S' \
	|| echo 'sgsh-tee -s | {{ diff & comm & }} $F'
	
	# ditto
	$(SGSHPATH)/bash --sgsh -c ' \
		grep -l -L -w -v match $(PSDIR)/f $(PSDIR)/F \
		| {{ \
			diff - - & \
			comm - - & \
		}} > $(PSDIR)/grep_diff_comm.outb' 2>/dev/null \
	&& (diff $(PSDIR)/grep_diff_comm.outb \
		$(PSDIR)/grep_diff_comm.success1-bash >/dev/null \
	    || diff $(PSDIR)/grep_diff_comm.outb \
		$(PSDIR)/grep_diff_comm.success2-bash >/dev/null) \
	&& echo 'grep | {{ diff & comm & }} $S' \
	|| echo 'grep | {{ diff & comm & }} $F'
	
	$(SGSHPATH)/bash --sgsh -c ' \
		comm $(PSDIR)/f4ss $(PSDIR)/f5ss \
		| {{ \
			paste - $(PSDIR)/p1 & \
			join - $(PSDIR)/j2 & \
			diff - $(PSDIR)/d3 & \
		}} > $(PSDIR)/comm_paste_join_diff.outb' 2>/dev/null \
	&& (diff $(PSDIR)/comm_paste_join_diff.outb \
		$(PSDIR)/comm_paste_join_diff.success1 >/dev/null \
	|| diff $(PSDIR)/comm_paste_join_diff.outb \
		$(PSDIR)/comm_paste_join_diff.success2 > /dev/null) \
	&& echo 'comm | {{ paste & join & diff & }} $S' \
	|| echo 'comm | {{ paste & join & diff & }} $F'
	
	$(SGSHPATH)/bash --sgsh -c ' \
		{{ \
			sort $(PSDIR)/f4s 2>/dev/null & \
			sort $(PSDIR)/f5s 2>/dev/null & \
		}} \
		| comm - - \
		> $(PSDIR)/sort_sort_comm.outb' 2>/dev/null \
	&& diff $(PSDIR)/sort_sort_comm.outb \
		$(PSDIR)/sort_sort_comm.success \
	&& echo '{{ sort & sort & }} | comm $S' \
	|| echo '{{ sort & sort & }} | comm $F'
	
	$(SGSHPATH)/bash --sgsh -c ' \
		{{ \
			sort $(PSDIR)/f4s & \
			sort $(PSDIR)/f5s & \
		}} \
		| comm - - \
		| {{ \
			paste - $(PSDIR)/p1 & \
			join - $(PSDIR)/j2 & \
			diff - $(PSDIR)/d3 & \
		}} > $(PSDIR)/sort_sort_comm_paste_join_diff.outb' \
			2>/dev/null \
	&& (diff $(PSDIR)/sort_sort_comm_paste_join_diff.outb \
		$(PSDIR)/sort_sort_comm_paste_join_diff.success1 >/dev/null \
	|| diff $(PSDIR)/sort_sort_comm_paste_join_diff.outb \
		$(PSDIR)/sort_sort_comm_paste_join_diff.success2 >/dev/null) \
	&& echo '{{ sort & sort & }} | comm | {{ paste & join & diff & }} $S' \
	|| echo '{{ sort & sort & }} | comm | {{ paste & join & diff & }} $F'
	
	echo " $BPaper examples under ../example:${EC}"

	# No diff, just check execution exit status
	$(SGSHPATH)/bash --sgsh $(XDIR)/dir.sh \
		>$(PSDIR)/dir.outb 2>/dev/null \
	&& echo 'dir.sh $S' \
	|| echo 'dir.sh $F'
	
	# ditto
	cat $(PSDIR)/data.csv | \
	$(SGSHPATH)/bash --sgsh $(XDIR)/reorder-columns.sh \
		>$(PSDIR)/reorder-columns.outb 2>/dev/null \
	&& echo 'reorder-columns.sh $S' \
	|| echo 'reorder-columns.sh $F'
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/manage-results.sh $(PSDIR) \
		>$(PSDIR)/manage-results.outb 2>/dev/null \
	&& echo 'manage-results.sh $S' \
	|| echo 'manage-results.sh $F'
	
	# ditto
	cat Readme.md | \
	$(SGSHPATH)/bash --sgsh $(XDIR)/compress-compare.sh \
		>$(PSDIR)/compress-compare.outb 2>/dev/null \
	&& echo 'compress-compare.sh $S' \
	|| echo 'compress-compare.sh $F'
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/commit-stats.sh Readme.md \
		>$(PSDIR)/commit-stats.outb 2>/dev/null \
	&& echo 'commit-stats.sh $S' \
	|| echo 'commit-stats.sh $F'
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/duplicate-files.sh \
		>$(PSDIR)/duplicate-files.outb 2>/dev/null \
	&& echo 'duplicate-files.sh $S' \
	|| echo 'duplicate-files.sh $F'
	
	# ditto
	cat Readme.md | \
	$(SGSHPATH)/bash --sgsh $(XDIR)/spell-highlight.sh \
		>$(PSDIR)/spell-highlight.outb 2>/dev/null \
	&& echo 'spell-highlight.sh $S' \
	|| echo 'spell-highlight.sh $F'
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/static-functions.sh diffutils \
		>$(PSDIR)/static-functions.outb 2>/dev/null \
	&& echo 'static-functions.sh $S' \
	|| echo 'static-functions.sh $F'
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/word-properties.sh Readme.md \
		>$(PSDIR)/word-properties.outb 2>/dev/null \
	&& echo 'word-properties.sh $S' \
	|| echo 'word-properties.sh $F'
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/map-hierarchy.sh tar tar-p100 tarp \
		>$(PSDIR)/map-hierarchy.outb 2>/dev/null \
	&& echo 'map-hierarchy.sh $S' \
	|| echo 'map-hierarchy.sh $F'
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/text-properties.sh Readme.md \
		>$(PSDIR)/text-properties.outb 2>/dev/null \
	&& echo 'text-properties.sh $S' \
	|| echo 'text-properties.sh $F'
	
	# ditto
	cd coreutils && $(SGSHPATH)/bash --sgsh ../$(XDIR)/committer-plot.sh \
		>../$(PSDIR)/committer-plot.outb 2>/dev/null \
	&& echo 'committer-plot.sh $S' \
	|| echo 'committer-plot.sh $F'
	# XXX: sgsh-writeval hangs
	pkill -f sgsh-writeval
	
	# ditto
	$(SGSHPATH)/bash --sgsh $(XDIR)/code-metrics.sh \
		>$(PSDIR)/code-metrics.outb 2>/dev/null \
	&& echo 'code-metrics.sh $S' \
	|| echo 'code-metrics.sh $F'
	
	# ditto
	cat $(PSDIR)/access.log | \
	$(SGSHPATH)/bash --sgsh $(XDIR)/web-log-report.sh \
		>$(PSDIR)/web-log-report.outb 2>/dev/null \
	&& echo 'web-log-report.sh $S' \
	|| echo 'web-log-report.sh $F'
	
test-pseudo: test-clean
	echo "$B\nPseudo shell tests:${EC}"
	
	cd $(PSDIR) && \
	python simple-shell.py secho_paste.sgsh secho_paste.out \
		2>/dev/null \
	&& diff secho_paste.out secho_paste.success \
	&& echo 'secho | paste $S' \
	|| echo 'secho | paste $F'

	cd $(PSDIR) && \
	python simple-shell.py wrap-cat_comm_sort.sgsh wrap-cat_comm_sort.out \
		2>/dev/null \
	&& diff wrap-cat_comm_sort.out wrap-cat_comm_sort.success \
	&& echo 'sgsh-wrap cat | comm | sort $S' \
	|| echo 'sgsh-wrap cat | comm | sort $F'
	
	cd $(PSDIR) && \
	python simple-shell.py comm_sort.sgsh comm_sort.out \
		2>/dev/null \
	&& diff comm_sort.out comm_sort.success \
	&& echo 'comm | sort $S' \
	|| echo 'comm | sort $F'
	
	cd $(PSDIR) && \
	python simple-shell.py comm_paste.sgsh comm_paste.out \
		2>/dev/null \
	&& diff comm_paste.out comm_paste.success \
	&& echo 'comm | paste $S' \
	|| echo 'comm | paste $F'
	
	cd $(PSDIR) && \
	python simple-shell.py join_sort.sgsh join_sort.out \
		2>/dev/null \
	&& diff join_sort.out join_sort.success \
	&& echo 'join | sort $S' \
	|| echo 'join | sort $F'
	
	cd $(PSDIR) && \
	python simple-shell.py paste_diff.sgsh paste_diff.out \
		2>/dev/null \
	&& diff paste_diff.out paste_diff.success \
	&& echo 'paste | diff $S' \
	|| echo 'paste | diff $F'
	
	cd $(PSDIR) && \
	python simple-shell.py grep_diff.sgsh grep_diff.out \
		2>/dev/null \
	&& diff grep_diff.out grep_diff.success \
	&& echo 'grep | diff $S' \
	|| echo 'grep | diff $F'
	
	cd $(PSDIR) && \
	python simple-shell.py grep_comm.sgsh grep_comm.out \
		2>/dev/null \
	&& diff grep_comm.out grep_comm.success \
	&& echo 'grep | comm $S' \
	|| echo 'grep | comm $F'
	
	cd $(PSDIR) && \
	python simple-shell.py join_sort_diff.sgsh join_sort_diff.out \
		2>/dev/null \
	&& diff join_sort_diff.out join_sort_diff.success \
	&& echo 'join | sort | diff $S' \
	|| echo 'join | sort | diff $F'
	
	cd $(PSDIR) && \
	python simple-shell.py dir-plain.sgsh dir-plain.out \
		2>/dev/null \
	&& echo 'ls -n | {{ awk & awk & }} $S' \
	|| echo 'ls -n | {{ awk & awk & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py secho_secho_fgrep.sgsh secho_secho_fgrep.out \
		2>/dev/null \
	&& diff $(PSDIR)/secho_secho_fgrep.out \
		$(PSDIR)/secho_secho_fgrep.success \
	&& echo '{{ secho & secho & }} | grep -F $S' \
	|| echo '{{ secho & secho & }} | grep -F $F'
	
	cd $(PSDIR) && \
	python simple-shell.py tee-copy_diff_comm.sgsh tee-copy_diff_comm.out \
		2>/dev/null \
	&& (diff $(PSDIR)/tee-copy_diff_comm.out \
			$(PSDIR)/tee-copy_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-copy_diff_comm.out \
			$(PSDIR)/tee-copy_diff_comm.success2 >/dev/null) \
	&& echo 'sgsh-tee | {{ diff & comm & }} $S' \
	|| echo 'sgsh-tee | {{ diff & comm & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py tee-scatter_diff_comm.sgsh tee-scatter_diff_comm.out \
		2>/dev/null \
	&& (diff $(PSDIR)/tee-scatter_diff_comm.out \
			$(PSDIR)/tee-scatter_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-scatter_diff_comm.out \
			$(PSDIR)/tee-scatter_diff_comm.success2 >/dev/null) \
	&& echo 'sgsh-tee -s | {{ diff & comm & }} $S' \
	|| echo 'sgsh-tee -s | {{ diff & comm & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py grep_diff_comm.sgsh grep_diff_comm.out \
		2>/dev/null \
	&& ($(STD)/diff grep_diff_comm.out grep_diff_comm.success1 >/dev/null \
	    || $(STD)/diff grep_diff_comm.out grep_diff_comm.success2 >/dev/null) \
	&& echo 'grep | {{ diff & comm & }} $S' \
	|| echo 'grep | {{ diff & comm & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py sort_sort_comm.sgsh sort_sort_comm.out \
		2>/dev/null \
	&& $(STD)/diff sort_sort_comm.out sort_sort_comm.success \
	&& echo '{{ sort & sort & }} | comm $S' \
	|| echo '{{ sort & sort & }} | comm $F'
	
	cd $(PSDIR) && \
	python simple-shell.py comm_paste_join_diff.sgsh \
		comm_paste_join_diff.out 2>/dev/null \
	&& (diff comm_paste_join_diff.out \
		comm_paste_join_diff.success1 >/dev/null \
	|| diff comm_paste_join_diff.out \
		comm_paste_join_diff.success2 > /dev/null) \
	&& echo 'comm | {{ paste & join & diff & }} $S' \
	|| echo 'comm | {{ paste & join & diff & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py \
		sort_sort_comm_paste_join_diff.sgsh \
		sort_sort_comm_paste_join_diff.out \
		2>/dev/null \
	&& (diff sort_sort_comm_paste_join_diff.out \
		sort_sort_comm_paste_join_diff.success1 >/dev/null \
	|| diff sort_sort_comm_paste_join_diff.out \
		sort_sort_comm_paste_join_diff.success2 >/dev/null) \
	&& echo '{{ sort & sort & }} | comm | {{ paste & join & diff & }} $S' \
	|| echo '{{ sort & sort & }} | comm | {{ paste & join & diff & }} $F'

clean:
	cd coreutils && make clean
	cd diffutils && make clean
	cd gawk && make clean
	cd grep && make clean
	cd parallel && make clean
	cd sed && make clean
	cd tar && make clean

