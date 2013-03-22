#!/bin/sh
#
# Format the examples into Bootstrap HTML
#

SECTION='compress-compare
	code-metrics
	duplicate-files
	word-properties
	parallel-logresolve
	web-log-stats
	map-hierarchy
	dir'


for NAME in ${SECTION}
do
	TITLE="`sed -n 's/^# SYNOPSIS //p' example/$NAME.sh`"
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
