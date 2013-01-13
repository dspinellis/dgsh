test: sgsh
	./sgsh -k example/code-metrics.sh test/code-metrics/in/ >test/code-metrics/out.test
	diff test/code-metrics/out.ok test/code-metrics/out.test

sgsh: sgsh.pl
	perl -c sgsh.pl
	install sgsh.pl sgsh
