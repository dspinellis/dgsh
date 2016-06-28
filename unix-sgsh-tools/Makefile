PSEUDOSHELLDIR=../simple-shell

configure:
	cd coreutils && ./bootstrap --gnulib-srcdir=`pwd`/../gnulib && ./configure --prefix=`pwd`/..
	cd diffutils && ./bootstrap --gnulib-srcdir=`pwd`/../gnulib && ./configure --prefix=`pwd`/..
	cd gawk && ./bootstrap.sh && ./configure --prefix=`pwd`/..
	cd grep && ./bootstrap --gnulib-srcdir=`pwd`/../gnulib && ./configure --prefix=`pwd`/..
	cd parallel && ./configure --prefix=`pwd`/..
	cd sed && ./bootstrap --gnulib-srcdir=`pwd`/../gnulib && ./configure --prefix=`pwd`/..
	cd tar && ./bootstrap --gnulib-srcdir=`pwd`/../gnulib && ./configure --prefix=`pwd`/..

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
	python $(PSEUDOSHELLDIR)/simple-shell.py comm_sort.sgsh && \
	sleep 1 && \
	echo "\nThe above output should match the following:\n" && \
	cat comm_sort.success

clean:
	cd coreutils && make clean
	cd diffutils && make clean
	cd gawk && make clean
	cd grep && make clean
	cd parallel && make clean
	cd sed && make clean
	cd tar && make clean

