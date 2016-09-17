#
# SYNOPSIS Plot git committer activity over time
# DESCRIPTION
# Process the git history, and create two PNG diagrams depicting
# committer activity over time. The most active committers appear
# at the center vertical of the diagram.
# Demonstrates image processing, mixining of synchronous and
# asynchronous processing in a scatter block, and the use of an
# sgsh-compliant join command.
#
#  Copyright 2013 Diomidis Spinellis
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#


# Commit history in the form of ascending Unix timestamps, emails
git log --pretty=tformat:'%at %ae' |
awk 'NF == 2 && $1 > 100000 && $1 < '`date +%s` |
sort -n |
sgsh-tee |
{{
	{{
		# Calculate number of committers
		awk '{print $2}' |
		sort -u |
		wc -l |
		sgsh-writeval -s committers &

		# Calculate number of days in window
		tail -1 |
		awk '{print $1}' |
		sgsh-writeval -s last &

		head -1 |
		awk '{print $1}' |
		sgsh-writeval -s first &
	}} &

	# Place committers left/right according to the number of their commits
	awk '{print $2}' |
	sort |
	uniq -c |
	sort -n |
	bash --sgsh-negotiate -c '/usr/bin/awk "BEGIN {l = 0; r = "`sgsh-readval -l -x -s committers`";}
			{print NR % 2 ? l++ : --r, \$2}"' |
	sort -k2 &

	sort -k2 &
}} |
# Join committer positions with commit time stamps
join -j 2 - - |
# Order by time
sort -k 2n |
sgsh-tee |
{{
	# Create portable bitmap
	echo 'P1' &
	bash --sgsh-negotiate -c '/bin/echo "`sgsh-readval -l -x -s committers` `sgsh-readval -l -x -s last` `sgsh-readval -l -x -s first`"' &
	bash --sgsh-negotiate -c 'perl -na -e "
	BEGIN { @empty["`sgsh-readval -l -x -s committers`" - 1] = 0; @committers = @empty; }
		sub out { print join(\"\", map(\$_ ? \"1\" : \"0\", @committers)), \"\n\"; }

		\$day = int(\$F[1] / 60 / 60 / 24);
		\$pday = \$day if (!defined(\$pday));

		while (\$day != \$pday) {
			out();
			@committers = @empty;
			\$pday++;
		}

		\$committers[\$F[2]] = 1;

		END { out(); }
		"' &
}} |
sgsh-tee |
# Enlarge points into discs through morphological convolution
pgmmorphconv -erode <(
/bin/cat <<EOF
P1
7 7
0 0 0 1 0 0 0
0 0 1 1 1 0 0
0 1 1 1 1 1 0
1 1 1 1 1 1 1
0 1 1 1 1 1 0
0 0 1 1 1 0 0
0 0 0 1 0 0 0
EOF
) |
sgsh-tee |
{{
	# Full-scale image
	pnmtopng & #>large.png &
	# A smaller image
	pamscale -width 640 |
	pnmtopng & #>small.png &
}}
