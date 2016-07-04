PSEUDOSHELLDIR=../simple-shell

# If not cloned repo with --recursive use this target to
# clone the submodule repos
get-submodules:
	git submodule update --init --recursive coreutils diffutils gawk grep parallel sed tar

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

install:
	cd coreutils && make install
	cd diffutils && make install
	cd gawk && make install
	cd grep && make install
	cd parallel && make install
	cd sed && make install
	cd tar && make install


test:
	cd $(PSEUDOSHELLDIR) && \
	python simple-shell.py comm_sort.sgsh comm_sort.out 2>/dev/null && \
	diff comm_sort.out comm_sort.success && \
	echo "Test comm | sort successful" || \
	echo "Test comm | sort failed"
	cd $(PSEUDOSHELLDIR) && \
	python simple-shell.py comm_paste.sgsh comm_paste.out 2>/dev/null && \
	diff comm_paste.out comm_paste.success && \
	echo "Test comm | paste successful" || \
	echo "Test comm | paste failed"
	cd $(PSEUDOSHELLDIR) && \
	python simple-shell.py join_sort.sgsh join_sort.out 2>/dev/null && \
	diff join_sort.out join_sort.success && \
	echo "Test join | sort successful" || \
	echo "Test join | sort failed"
	cd $(PSEUDOSHELLDIR) && \
	python simple-shell.py paste_diff.sgsh paste_diff.out 2>/dev/null && \
	diff paste_diff.out paste_diff.success && \
	echo "Test paste | diff successful" || \
	echo "Test paste | diff failed"
	cd $(PSEUDOSHELLDIR) && \
	python simple-shell.py join_sort_diff.sgsh join_sort_diff.out 2>/dev/null && \
	diff join_sort_diff.out join_sort_diff.success && \
	echo "Test join | sort | diff successful" || \
	echo "Test join| sort | diff failed"
clean:
	cd coreutils && make clean
	cd diffutils && make clean
	cd gawk && make clean
	cd grep && make clean
	cd parallel && make clean
	cd sed && make clean
	cd tar && make clean

