PSDIR=../simple-shell
# TOOLDIR
TD=unix-sgsh-tools
BIN=bin
SGSH=..

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
	cd coreutils && ./bootstrap && ./configure --prefix=`pwd`/..
	cd diffutils && ./bootstrap && ./configure --prefix=`pwd`/..
	cd gawk && ./bootstrap.sh && ./configure --prefix=`pwd`/..
	cd grep && ./bootstrap && ./configure --prefix=`pwd`/..
	cd parallel && ./configure --prefix=`pwd`/..
	cd sed && ./bootstrap && ./configure --prefix=`pwd`/..
	cd tar && ./bootstrap && ./configure --prefix=`pwd`/..

make:
	cd coreutils && make
	cd diffutils && make
	cd gawk && make
	cd grep && make
	cd parallel && make
	cd sed && make
	cd tar && make
	$(CC) $(CFLAGS) $(PATHS) simple_echo.c -o secho $(LIBS)

install:
	cd coreutils && make install
	cd diffutils && make install
	cd gawk && make install
	cd grep && make install
	cd parallel && make install
	cd sed && make install
	cd tar && make install
	mv secho bin/


test-clean-bash:
	-rm $(PSDIR)/*.outb

test-clean:
	-rm $(PSDIR)/*.out

test: test-bash test-pseudo

test-bash: test-clean
	echo "$B\nBash tests:${EC}"
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/secho hello \
		| $(BIN)/paste - $(PSDIR)/world \
		> $(PSDIR)/secho_paste.outb' 2>/dev/null \
	&& diff $(PSDIR)/secho_paste.outb $(PSDIR)/secho_paste.success \
	&& echo 'secho | paste $S' \
	|| echo 'secho | paste $F'

	$(BIN)/bash --sgsh -c ' \
		$(SGSH)/sgsh-wrap cat $(PSDIR)/f1s \
		| $(BIN)/comm - $(PSDIR)/f2s \
		| $(BIN)/sort \
		> $(PSDIR)/wrap-cat_comm_sort.outb' 2>/dev/null \
	&& diff $(PSDIR)/wrap-cat_comm_sort.outb \
		$(PSDIR)/wrap-cat_comm_sort.success \
	&& echo 'sgsh-wrap cat | comm | sort $S' \
	|| echo 'sgsh-wrap cat | comm | sort $F'
	
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/sort > $(PSDIR)/comm_sort.outb' 2>/dev/null \
	&& diff $(PSDIR)/comm_sort.outb $(PSDIR)/comm_sort.success \
	&& echo 'comm | sort $S' \
	|| echo 'comm | sort $F'
	
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/paste - - - \
		> $(PSDIR)/comm_paste.outb' 2>/dev/null \
	&& diff $(PSDIR)/comm_paste.outb \
		$(PSDIR)/comm_paste.success \
	&& echo 'comm | paste $S' \
	|| echo 'comm | paste $F'
	
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/join $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/sort \
		> $(PSDIR)/join_sort.outb' 2>/dev/null \
	&& diff $(PSDIR)/join_sort.outb \
		$(PSDIR)/join_sort.success \
	&& echo 'join | sort $S' \
	|| echo 'join | sort $F'
	
	# || instead of && in the following test because diff's exit 
	# status becomes the command's exit status
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/paste $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/diff $(PSDIR)/f1s - \
		> $(PSDIR)/paste_diff.outb' 2>/dev/null \
	|| diff $(PSDIR)/paste_diff.outb $(PSDIR)/paste_diff.success \
	&& echo 'paste | diff $S' \
	|| echo 'paste | diff $F'
	
	#ditto
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/grep -v -w match $(PSDIR)/f $(PSDIR)/F \
		| $(BIN)/diff - - \
		> $(PSDIR)/grep_diff.outb' 2>/dev/null \
	|| diff $(PSDIR)/grep_diff.outb $(PSDIR)/grep_diff.success-bash \
	&& echo 'grep | diff $S' \
	|| echo 'grep | diff $F'
	
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/grep -l -L match $(PSDIR)/f $(PSDIR)/F \
		| $(BIN)/comm - - \
		> $(PSDIR)/grep_comm.outb' 2>/dev/null \
	|| diff $(PSDIR)/grep_comm.outb $(PSDIR)/grep_comm.success \
	&& echo 'grep | comm $S' \
	|| echo 'grep | comm $F'
	
	# ditto
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/join $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/sort \
		| $(BIN)/diff $(PSDIR)/f3s - \
		> $(PSDIR)/join_sort_diff.outb' 2>/dev/null \
	|| diff $(PSDIR)/join_sort_diff.outb \
		$(PSDIR)/join_sort_diff.success \
	&& echo 'join | sort | diff $S' \
	|| echo 'join | sort | diff $F'
	
	# No diff, just check execution exit status
	# Escape parentheses in awk inline script
	$(BIN)/bash --sgsh -c ' \
		$(SGSH)/sgsh-wrap ls -n \
		| $(SGSH)/sgsh-tee \
		| {{ \
			$(SGSH)/sgsh-wrap awk '!/^total/ {print $6, $7, $8, $1, sprintf\("%8d", $5\), $9}' & \
			$(SGSH)/sgsh-wrap awk '{s += $5} END {printf\("%d bytes", s\)}' & \
		}} > $(PSDIR)/dir-plain.outb'
	exit 1
	#\
	#&& echo 'wc -l | {{ awk & awk & }} $S' \
	#|| echo 'wc -l | {{ awk & awk & }} $F'
	
	# ditto
	$(BIN)/bash --sgsh -c ' \
		{{ \
			$(BIN)/secho match & \
			$(BIN)/secho not & \
		}} \
		| $(BIN)/grep -F -h match - - \
	> $(PSDIR)/secho_secho_fgrep.outb' 2>/dev/null \
	&& diff $(PSDIR)/secho_secho_fgrep.outb \
		$(PSDIR)/secho_secho_fgrep.success \
	&& echo '{{ secho & secho & }} | grep -F $S' \
	|| echo '{{ secho & secho & }} | grep -F $F'
	
	$(BIN)/bash --sgsh -c ' \
		$(SGSH)/sgsh-tee -i $(PSDIR)/hello \
		| {{ \
			$(BIN)/diff $(PSDIR)/world - & \
			$(BIN)/comm $(PSDIR)/hello - & \
		}} > $(PSDIR)/tee-copy_diff_comm.outb' 2>/dev/null \
	&& (diff $(PSDIR)/tee-copy_diff_comm.outb \
			$(PSDIR)/tee-copy_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-copy_diff_comm.outb \
			$(PSDIR)/tee-copy_diff_comm.success2 >/dev/null) \
	&& echo 'sgsh-tee | {{ diff & comm & }} $S' \
	|| echo 'sgsh-tee | {{ diff & comm & }} $F'
	
	$(BIN)/bash --sgsh -c ' \
		$(SGSH)/sgsh-tee -i $(PSDIR)/hello -s \
		| {{ \
			$(BIN)/diff $(PSDIR)/world - & \
			$(BIN)/comm $(PSDIR)/hello - & \
		}} > $(PSDIR)/tee-scatter_diff_comm.outb' 2>/dev/null \
	&& (diff $(PSDIR)/tee-scatter_diff_comm.outb \
			$(PSDIR)/tee-scatter_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-scatter_diff_comm.outb \
			$(PSDIR)/tee-scatter_diff_comm.success2 >/dev/null) \
	&& echo 'sgsh-tee -s | {{ diff & comm & }} $S' \
	|| echo 'sgsh-tee -s | {{ diff & comm & }} $F'
	
	# ditto
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/grep -l -L -w -v match $(PSDIR)/f $(PSDIR)/F \
		| {{ \
			$(BIN)/diff - - & \
			$(BIN)/comm - - & \
		}} > $(PSDIR)/grep_diff_comm.outb' 2>/dev/null \
	&& (diff $(PSDIR)/grep_diff_comm.outb \
		$(PSDIR)/grep_diff_comm.success1-bash >/dev/null \
	    || diff $(PSDIR)/grep_diff_comm.outb \
		$(PSDIR)/grep_diff_comm.success2-bash >/dev/null) \
	&& echo 'grep | {{ diff & comm & }} $S' \
	|| echo 'grep | {{ diff & comm & }} $F'
	
	bin/bash --sgsh -c ' \
		bin/comm ../simple-shell/f4ss ../simple-shell/f5ss \
		| {{ \
			bin/paste - ../simple-shell/p1 & \
			bin/join - ../simple-shell/j2 & \
			bin/diff - ../simple-shell/d3 & \
		}} > $(PSDIR)/comm_paste_join_diff.outb' 2>/dev/null \
	&& (diff $(PSDIR)/comm_paste_join_diff.outb \
		$(PSDIR)/comm_paste_join_diff.success1 >/dev/null \
	|| diff $(PSDIR)/comm_paste_join_diff.outb \
		$(PSDIR)/comm_paste_join_diff.success2 > /dev/null) \
	&& echo 'comm | {{ paste & join & diff & }} $S' \
	|| echo 'comm | {{ paste & join & diff & }} $F'
	
	bin/bash --sgsh -c ' \
		{{ \
			bin/sort ../simple-shell/f4s 2>/dev/null & \
			bin/sort ../simple-shell/f5s 2>/dev/null & \
		}} \
		| bin/comm - - \
		> $(PSDIR)/sort_sort_comm.outb' 2>/dev/null \
	&& diff $(PSDIR)/sort_sort_comm.outb \
		$(PSDIR)/sort_sort_comm.success \
	&& echo '{{ sort & sort & }} | comm $S' \
	|| echo '{{ sort & sort & }} | comm $F'
	
	bin/bash --sgsh -c ' \
		{{ \
			bin/sort ../simple-shell/f4s & \
			bin/sort ../simple-shell/f5s & \
		}} \
		| bin/comm - - \
		| {{ \
			bin/paste - ../simple-shell/p1 & \
			bin/join - ../simple-shell/j2 & \
			bin/diff - ../simple-shell/d3 & \
		}} > $(PSDIR)/sort_sort_comm_paste_join_diff.outb' \
			2>/dev/null \
	&& (diff $(PSDIR)/sort_sort_comm_paste_join_diff.outb \
		$(PSDIR)/sort_sort_comm_paste_join_diff.success1 >/dev/null \
	|| diff $(PSDIR)/sort_sort_comm_paste_join_diff.outb \
		$(PSDIR)/sort_sort_comm_paste_join_diff.success2 >/dev/null) \
	&& echo '{{ sort & sort & }} | comm | {{ paste & join & diff & }} $S' \
	|| echo '{{ sort & sort & }} | comm | {{ paste & join & diff & }} $F'

test-pseudo: test-clean
	echo "$B\nPseudo shell tests:${EC}"
	cd $(PSDIR) && \
	python simple-shell.py secho_paste.sgsh secho_paste.out \
		>/dev/null 2>/dev/null \
	&& diff secho_paste.out secho_paste.success \
	&& echo 'secho | paste $S' \
	|| echo 'secho | paste $F'

	cd $(PSDIR) && \
	python simple-shell.py wrap-cat_comm_sort.sgsh wrap-cat_comm_sort.out \
		>/dev/null 2>/dev/null \
	&& diff wrap-cat_comm_sort.out wrap-cat_comm_sort.success \
	&& echo 'sgsh-wrap cat | comm | sort $S' \
	|| echo 'sgsh-wrap cat | comm | sort $F'
	
	cd $(PSDIR) && \
	python simple-shell.py comm_sort.sgsh comm_sort.out \
		>/dev/null 2>/dev/null \
	&& diff comm_sort.out comm_sort.success \
	&& echo 'comm | sort $S' \
	|| echo 'comm | sort $F'
	
	cd $(PSDIR) && \
	python simple-shell.py comm_paste.sgsh comm_paste.out \
		>/dev/null 2>/dev/null \
	&& diff comm_paste.out comm_paste.success \
	&& echo 'comm | paste $S' \
	|| echo 'comm | paste $F'
	
	cd $(PSDIR) && \
	python simple-shell.py join_sort.sgsh join_sort.out \
		>/dev/null 2>/dev/null \
	&& diff join_sort.out join_sort.success \
	&& echo 'join | sort $S' \
	|| echo 'join | sort $F'
	
	cd $(PSDIR) && \
	python simple-shell.py paste_diff.sgsh paste_diff.out \
		>/dev/null 2>/dev/null \
	&& diff paste_diff.out paste_diff.success \
	&& echo 'paste | diff $S' \
	|| echo 'paste | diff $F'
	
	cd $(PSDIR) && \
	python simple-shell.py grep_diff.sgsh grep_diff.out \
		>/dev/null 2>/dev/null \
	&& diff grep_diff.out grep_diff.success \
	&& echo 'grep | diff $S' \
	|| echo 'grep | diff $F'
	
	cd $(PSDIR) && \
	python simple-shell.py grep_comm.sgsh grep_comm.out \
		>/dev/null 2>/dev/null \
	&& diff grep_comm.out grep_comm.success \
	&& echo 'grep | comm $S' \
	|| echo 'grep | comm $F'
	
	cd $(PSDIR) && \
	python simple-shell.py join_sort_diff.sgsh join_sort_diff.out \
		>/dev/null 2>/dev/null \
	&& diff join_sort_diff.out join_sort_diff.success \
	&& echo 'join | sort | diff $S' \
	|| echo 'join | sort | diff $F'
	
	cd $(PSDIR) && \
	python simple-shell.py dir-plain.sgsh dir-plain.out \
		>/dev/null 2>/dev/null \
	&& echo 'ls -n | {{ awk & awk & }} $S' \
	|| echo 'ls -n | {{ awk & awk & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py secho_secho_fgrep.sgsh secho_secho_fgrep.out \
		>/dev/null 2>/dev/null \
	&& diff $(PSDIR)/secho_secho_fgrep.out \
		$(PSDIR)/secho_secho_fgrep.success \
	&& echo '{{ secho & secho & }} | grep -F $S' \
	|| echo '{{ secho & secho & }} | grep -F $F'
	
	cd $(PSDIR) && \
	python simple-shell.py tee-copy_diff_comm.sgsh tee-copy_diff_comm.out \
		>/dev/null 2>/dev/null \
	&& (diff $(PSDIR)/tee-copy_diff_comm.out \
			$(PSDIR)/tee-copy_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-copy_diff_comm.out \
			$(PSDIR)/tee-copy_diff_comm.success2 >/dev/null) \
	&& echo 'sgsh-tee | {{ diff & comm & }} $S' \
	|| echo 'sgsh-tee | {{ diff & comm & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py tee-scatter_diff_comm.sgsh tee-scatter_diff_comm.out \
		>/dev/null 2>/dev/null \
	&& (diff $(PSDIR)/tee-scatter_diff_comm.out \
			$(PSDIR)/tee-scatter_diff_comm.success1 >/dev/null \
		|| diff $(PSDIR)/tee-scatter_diff_comm.out \
			$(PSDIR)/tee-scatter_diff_comm.success2 >/dev/null) \
	&& echo 'sgsh-tee -s | {{ diff & comm & }} $S' \
	|| echo 'sgsh-tee -s | {{ diff & comm & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py grep_diff_comm.sgsh grep_diff_comm.out \
		>/dev/null 2>/dev/null \
	&& (diff grep_diff_comm.out grep_diff_comm.success1 >/dev/null \
	    || diff grep_diff_comm.out grep_diff_comm.success2 >/dev/null) \
	&& echo 'grep | {{ diff & comm & }} $S' \
	|| echo 'grep | {{ diff & comm & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py sort_sort_comm.sgsh sort_sort_comm.out \
		>/dev/null 2>/dev/null \
	&& diff sort_sort_comm.out sort_sort_comm.success \
	&& echo '{{ sort & sort & }} | comm $S' \
	|| echo '{{ sort & sort & }} | comm $F'
	
	cd $(PSDIR) && \
	python simple-shell.py comm_paste_join_diff.sgsh \
		comm_paste_join_diff.out >/dev/null 2>/dev/null \
	&& (diff comm_paste_join_diff.out \
		comm_paste_join_diff.success1 >/dev/null \
	|| diff comm_paste_join_diff.out \
		comm_paste_join_diff.success2 > /dev/null) \
	&& echo 'comm | {{ paste & join & diff & }} $S' \
	|| echo 'comm | {{ paste & join & diff & }} $F'
	
	cd $(PSDIR) && \
	python simple-shell.py \
		sort_sort_comm_paste_join_diff.sgsh \
		sort_sort_comm_paste_join_diff.out >/dev/null \
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

