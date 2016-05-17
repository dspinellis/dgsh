prepare:
	git clone git://git.savannah.gnu.org/gnulib.git
	git clone git://git.savannah.gnu.org/coreutils.git
	git clone git://git.savannah.gnu.org/diffutils.git
	git clone git://git.savannah.gnu.org/gawk.git
	git clone git://git.savannah.gnu.org/grep.git
	git clone git://git.savannah.gnu.org/parallel.git
	git clone git://git.savannah.gnu.org/sed.git
	git clone git://git.savannah.gnu.org/tar.git

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

clean:
	cd coreutils && make clean
	cd diffutils && make clean
	cd gawk && make clean
	cd grep && make clean
	cd parallel && make clean
	cd sed && make clean
	cd tar && make clean
