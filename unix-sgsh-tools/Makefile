ADAPTED_TOOLS_SRC_DIR=sgsh-adapted

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

dev:
	#Only the first time uncomment the following two lines.
	#mv coreutils/src/comm.c coreutils/src/coreutils-comm.c
	#mv coreutils/src/join.c coreutils/src/coreutils-join.c
	#mv coreutils/src/paste.c coreutils/src/coreutils-paste.c
	#mv coreutils/src/sort.c coreutils/src/coreutils-sort.c
	#mv coreutils/src/Makefile coreutils/coreutils-Makefile
	#cp $(ADAPTED_TOOLS_SRC_DIR)/coreutils/Makefile coreutils/
	#cp $(ADAPTED_TOOLS_SRC_DIR)/coreutils/*.c coreutils/src/
	cd coreutils && make && make install

	#Only the first time uncomment the following two lines.
	#mv diffutils/src/diff.c diffutils/src/diffutils-diff.c
	#mv diffutils/src/Makefile diffutils/src/diffutils-Makefile
	#cp $(ADAPTED_TOOLS_SRC_DIR)/diffutils/* diffutils/src/
	#cd diffutils && make && make install

	#Only the first time uncomment the following two lines.
	#mv grep/src/grep.c grep/src/grep-grep.c
	#mv grep/src/Makefile grep/src/grep-Makefile
	#cp $(ADAPTED_TOOLS_SRC_DIR)/grep/* grep/src/
	#cd grep && make && make install
