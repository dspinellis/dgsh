#!/bin/bash

set -x

brew install check
wget http://ftp.gnu.org/gnu/gettext/gettext-0.19.5.tar.xz
tar Jxvf gettext-0.19.5.tar.xz >/dev/null
cd gettext-0.19.5 && ./configure && make && sudo make install >/dev/null
cd ..
#git clone --depth=1 -b madagascar-devel-2016 https://github.com/ahay/src.git madagascar
#cd madagascar && rm -rf trip
#sudo ./configure --prefix=/usr/local && sudo make && sudo make install
#cd ..
mkdir nmrpipe && cd nmrpipe
wget https://www.ibbr.umd.edu/nmrpipe/install.com
wget https://www.ibbr.umd.edu/nmrpipe/binval.com
wget https://www.ibbr.umd.edu/nmrpipe/NMRPipeX.tZ
wget https://www.ibbr.umd.edu/nmrpipe/s.tZ
wget https://www.ibbr.umd.edu/nmrpipe/dyn.tZ
wget https://www.ibbr.umd.edu/nmrpipe/talos.tZ
wget http://spin.niddk.nih.gov/bax/software/smile/plugin.smile.tZ
chmod a+rx *.com && ./install.com >/dev/null
sudo install nmrbin.linux212_64/var2pipe nmrbin.linux212_64/nmrPipe /usr/local/bin >/dev/null
sudo install nmrbin.linux212_64/addNMR /usr/bin >/dev/null
cd ..
make config && make
