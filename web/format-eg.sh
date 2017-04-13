#!/bin/sh
#
# Format the examples into Bootstrap HTML
#

SECTION='compress-compare
	commit-stats
	code-metrics
	duplicate-files
	spell-highlight
	word-properties
	web-log-report
	text-properties
	static-functions
	map-hierarchy
	committer-plot
	parallel-word-count
	author-compare
	ft2d
	NMRPipe
	fft-block8
	set-operations
	reorder-columns
	dir'


for NAME in ${SECTION}
do
	TITLE="`sed -n 's/^# SYNOPSIS //p;s/^# TITLE //p' example/$NAME.sh | head -1`"
	if [ "$1" = '-c' ]
	then
		echo "<li> <a href='#$NAME'>$TITLE</a></li>"
	else
		cat <<EOF
<section id="$NAME"> <!-- {{{2 -->
<h2>$TITLE</h2>
</section>
<img src="$NAME-pretty.png" class="img-polaroid" alt="$TITLE" />
<!-- Extracted description -->
<p>
`sed -n '/^# DESCRIPTION/,/^#$/{;/^# DESCRIPTION/d;s/^# //p;}' example/$NAME.sh`
</p>
<!-- Extracted code -->
<pre class="prettyprint lang-bash">
`sed '1a\

2,/^$/d' example/$NAME.sh |
sed 's/&/\&amp;/g;s/</\&lt;/g;s/>/\&gt;/g'`
</pre>
EOF
	fi
done
