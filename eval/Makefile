include ../.config

all: data WebStats.class TextProperties.class
	sh perf-eval.sh

data: books.txt linux access.log linux.new linux.old emptydir access-small.log

# To ensure repeatability use specific hash for an old and a new Linux commit
LOLD=1da177e4c3f41524e886b7f1b8a0c1fc7321cac2
LNEW=899552d6e84babd24611fd36ac7051068cb1eb2d
ELOLD:=$(LOLD)
export ELOLD
ELNEW:=$(LNEW)
export ELNEW

# History of the United States by Charles A. Beard and Mary Ritter Beard
# The Adventures of Sherlock Holmes by Arthur Conan Doyle
# Les Misérables by Victor Hugo
books1.txt:
	curl http://www.gutenberg.org/files/16960/16960.txt \
	http://www.gutenberg.org/files/1661/1661.txt \
	http://www.gutenberg.org/files/135/135.txt >books1.txt

books.txt: books1.txt
	for i in `seq 1 10` ; do cat $? ; done >$@

access.log:
	curl ftp://ita.ee.lbl.gov/traces/clarknet_access_log_Aug28.gz |\
	gzip -dc >$@

access-small.log: access.log
	head -10 $? >$@

linux:
	git clone https://github.com/torvalds/linux.git

linux.new: linux
	(cd linux && git checkout $(LNEW))
	mkdir $@
	tar -C linux --exclude .git -cf - . | tar -C $@ -xf -
	(cd linux && git checkout master)
	touch $@

linux.old: linux
	(cd linux && git checkout $(LOLD))
	mkdir $@
	tar -C linux --exclude .git -cf - . | tar -C $@ -xf -
	(cd linux && git checkout master)
	touch $@

emptydir: linux
	mkdir $@
	cp linux/kernel/up.c $@/

emptygit:
	mkdir $@
	cd $@ ; git init ; echo hello >hello ; git add hello ; git commit -am 'Add hello'

WebStats.class: WebStats.java
	javac $?

TextProperties.class: TextProperties.java
	javac $?

clean:
	rm -rf `cat .gitignore`
