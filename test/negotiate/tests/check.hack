	# MFG: Hack for identifying and aggregating test results in log.
	# Goes in test/negotiate/tests/Makefile
	# Grep for 'xfail' to identify the block that the current
	# substitutes.
	results=`for b in $$bases; do echo $$b.log; done`; \
	test -n "$$results" || results=/dev/null; \
	all=` grep ":.\+:.:.\+:.\+:.\+: "           $$results | wc -l`; \
	pass=` grep "^.\+\.c:.\+:.:.\+:.\+:.\+: Passed"  $$results | wc -l`; \
	fail=` grep "^.\+\.c:.\+:.:.\+:.\+:.\+: Assertion .\+ failed:"  $$results | wc -l`; \
	skip=` grep "^.\+\.c:.\+:.:.\+:.\+:.\+: Skipped"  $$results | wc -l`; \
	xfail=`grep "^.\+\.c:.\+:.:.\+:.\+:.\+: XFailed" $$results | wc -l`; \
	xpass=`grep "^.\+\.c:.\+:.:.\+:.\+:.\+: XPassed" $$results | wc -l`; \
	error=`grep ":.\+:.:.\+:.\+:.\+: (after this point) Received" $$results | wc -l`; \
