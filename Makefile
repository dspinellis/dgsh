measure: measure-sg.sh
	perl scatter-gather.pl $? >$@

berkeley-sys.xls: old.out bsd.out freebsd.out
	sed -n '/sys/s/ *//gp' $? | tr : \\t >$@
