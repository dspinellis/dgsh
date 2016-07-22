PSDIR=../simple-shell
# TOOLDIR
TD=unix-sgsh-tools
BIN=bin
SGSH=..

CFLAGS+=-Wall
PATHS= -I.. -L..
LIBS= -lsgsh_negotiate

# If not cloned repo with --recursive use this target to
# clone the submodule repos
get-submodules:
	cd .. && git submodule update --init --recursive \
		$(TD)/bash \
		$(TD)/coreutils \
		$(TD)/diffutils \
		$(TD)/grep \
		$(TD)/parallel \
		$(TD)/sed

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


test-clean:
	-rm $(PSDIR)/*.out

test: test-clean test-bash test-pseudo

test-bash: test-clean
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/secho hello \
		| $(BIN)/paste - $(PSDIR)/world \
		> $(PSDIR)/secho_paste.out' 2>/dev/null \
	&& diff $(PSDIR)/secho_paste.out $(PSDIR)/secho_paste.success \
	&& echo "Test secho | paste successful" \
	|| echo "Test secho | paste failed"
	
	$(BIN)/bash --sgsh -c ' \
		$(SGSH)/sgsh-wrap cat $(PSDIR)/f1s \
		| $(BIN)/comm - $(PSDIR)/f2s \
		| $(BIN)/sort \
		> $(PSDIR)/wrap-cat_comm_sort.out' 2>/dev/null \
	&& diff $(PSDIR)/wrap-cat_comm_sort.out $(PSDIR)/wrap-cat_comm_sort.success \
	&& echo "Test sgsh-wrap cat | comm | sort successful" \
	|| echo "Test sgsh-wrap cat | comm | sort failed"
	
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/sort > $(PSDIR)/comm_sort.out' 2>/dev/null \
	&& diff $(PSDIR)/comm_sort.out $(PSDIR)/comm_sort.success \
	&& echo "Test comm | sort successful" \
	|| echo "Test comm | sort failed"
	
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/comm $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/paste - - - \
		> $(PSDIR)/comm_paste.out' 2>/dev/null \
	&& diff $(PSDIR)/comm_paste.out \
		$(PSDIR)/comm_paste.success \
	&& echo "Test comm | paste successful" \
	|| echo "Test comm | paste failed"
	
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/join $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/sort \
		> $(PSDIR)/join_sort.out' 2>/dev/null \
	&& diff $(PSDIR)/join_sort.out \
		$(PSDIR)/join_sort.success \
	&& echo "Test join | sort successful" \
	|| echo "Test join | sort failed"
	
	# || instead of && in the following test because diff's exit 
	# status becomes the command's exit status
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/paste $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/diff $(PSDIR)/f1s - \
		> $(PSDIR)/paste_diff.out' 2>/dev/null \
	|| diff $(PSDIR)/paste_diff.out $(PSDIR)/paste_diff.success \
	&& echo "Test paste | diff successful" \
	|| echo "Test paste | diff failed"
	
	# ditto
	$(BIN)/bash --sgsh -c ' \
		$(BIN)/join $(PSDIR)/f1s $(PSDIR)/f2s \
		| $(BIN)/sort \
		| $(BIN)/diff $(PSDIR)/f3s - \
		> $(PSDIR)/join_sort_diff.out' 2>/dev/null \
	|| diff $(PSDIR)/join_sort_diff.out \
		$(PSDIR)/join_sort_diff.success \
	&& echo "Test join | sort | diff successful" \
	|| echo "Test join | sort | diff failed"
	
	bin/bash --sgsh -c ' \
		bin/comm ../simple-shell/f4ss ../simple-shell/f5ss \
		| {{ \
			bin/paste - ../simple-shell/p1 & \
			bin/join - ../simple-shell/j2 & \
			bin/diff - ../simple-shell/d3 & \
		}} > $(PSDIR)/comm_paste_join_diff.out' 2>/dev/null \
	&& (diff $(PSDIR)/comm_paste_join_diff.out \
		$(PSDIR)/comm_paste_join_diff.success1 >/dev/null \
	|| diff $(PSDIR)/comm_paste_join_diff.out \
		$(PSDIR)/comm_paste_join_diff.success2 > /dev/null) \
	&& echo "Test comm | {{ paste & join & diff & }} successful" \
	|| echo "Test comm | {{ paste & join & diff & }} failed"
	
	bin/bash --sgsh -c ' \
		{{ \
			bin/sort ../simple-shell/f4s 2>/dev/null & \
			bin/sort ../simple-shell/f5s 2>/dev/null & \
		}} \
		| bin/comm - - \
		> $(PSDIR)/sort_sort_comm.out' 2>/dev/null \
	&& diff $(PSDIR)/sort_sort_comm.out \
		$(PSDIR)/sort_sort_comm.success \
	&& echo "Test {{ sort & sort & }} | comm successful" \
	|| echo "Test {{ sort & sort & }} | comm failed"
	
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
		}} > $(PSDIR)/sort_sort_comm_paste_join_diff.out' \
			2>/dev/null \
	&& (diff $(PSDIR)/sort_sort_comm_paste_join_diff.out \
		$(PSDIR)/sort_sort_comm_paste_join_diff.success1 >/dev/null \
	|| diff $(PSDIR)/sort_sort_comm_paste_join_diff.out \
		$(PSDIR)/sort_sort_comm_paste_join_diff.success2 >/dev/null) \
	&& echo "Test {{ sort & sort & }} | comm | {{ paste & join & diff & }} successful" \
	|| echo "Test {{ sort & sort & }} | comm | {{ paste & join & diff & }} failed"

test-pseudo: test-clean
	cd $(PSDIR) && \
	python simple-shell.py secho_paste.sgsh secho_paste.out \
		>/dev/null 2>/dev/null \
	&& diff secho_paste.out secho_paste.success \
	&& echo "Test secho | paste successful" \
	|| echo "Test secho | paste failed"

	cd $(PSDIR) && \
	python simple-shell.py wrap-cat_comm_sort.sgsh wrap-cat_comm_sort.out \
		>/dev/null 2>/dev/null \
	&& diff wrap-cat_comm_sort.out wrap-cat_comm_sort.success \
	&& echo "Test sgsh-wrap cat | comm | sort successful" \
	|| echo "Test sgsh-wrap cat | comm | sort failed"
	
	cd $(PSDIR) && \
	python simple-shell.py comm_sort.sgsh comm_sort.out \
		>/dev/null 2>/dev/null \
	&& diff comm_sort.out comm_sort.success \
	&& echo "Test comm | sort successful" \
	|| echo "Test comm | sort failed"
	
	cd $(PSDIR) && \
	python simple-shell.py comm_paste.sgsh comm_paste.out \
		>/dev/null 2>/dev/null \
	&& diff comm_paste.out comm_paste.success \
	&& echo "Test comm | paste successful" \
	|| echo "Test comm | paste failed"
	
	cd $(PSDIR) && \
	python simple-shell.py join_sort.sgsh join_sort.out \
		>/dev/null 2>/dev/null \
	&& diff join_sort.out join_sort.success \
	&& echo "Test join | sort successful" \
	|| echo "Test join | sort failed"
	
	cd $(PSDIR) && \
	python simple-shell.py paste_diff.sgsh paste_diff.out \
		>/dev/null 2>/dev/null \
	&& diff paste_diff.out paste_diff.success \
	&& echo "Test paste | diff successful" \
	|| echo "Test paste | diff failed"
	
	cd $(PSDIR) && \
	python simple-shell.py join_sort_diff.sgsh join_sort_diff.out \
		>/dev/null 2>/dev/null \
	&& diff join_sort_diff.out join_sort_diff.success \
	&& echo "Test join | sort | diff successful" \
	|| echo "Test join | sort | diff failed"
	
	cd $(PSDIR) && \
	python simple-shell.py sort_sort_comm.sgsh sort_sort_comm.out \
		>/dev/null 2>/dev/null \
	&& diff sort_sort_comm.out sort_sort_comm.success \
	&& echo "Test {{ sort & sort & }} | comm successful" \
	|| echo "Test {{ sort & sort & }} | comm failed"
	
	cd $(PSDIR) && \
	python simple-shell.py comm_paste_join_diff.sgsh \
		comm_paste_join_diff.out >/dev/null 2>/dev/null \
	&& (diff comm_paste_join_diff.out \
		comm_paste_join_diff.success1 >/dev/null \
	|| diff comm_paste_join_diff.out \
		comm_paste_join_diff.success2 > /dev/null) \
	&& echo "Test comm | {{ paste & join & diff & }} successful" \
	|| echo "Test comm | {{ paste & join & diff & }} failed"
	
	cd $(PSDIR) && \
	python simple-shell.py \
		sort_sort_comm_paste_join_diff.sgsh \
		sort_sort_comm_paste_join_diff.out >/dev/null \
		2>/dev/null \
	&& (diff sort_sort_comm_paste_join_diff.out \
		sort_sort_comm_paste_join_diff.success1 >/dev/null \
	|| diff sort_sort_comm_paste_join_diff.out \
		sort_sort_comm_paste_join_diff.success2 >/dev/null) \
	&& echo "Test {{ sort & sort & }} | comm | {{ paste & join & diff & }} successful" \
	|| echo "Test {{ sort & sort & }} | comm | {{ paste & join & diff & }} failed"

clean:
	cd coreutils && make clean
	cd diffutils && make clean
	cd gawk && make clean
	cd grep && make clean
	cd parallel && make clean
	cd sed && make clean
	cd tar && make clean

