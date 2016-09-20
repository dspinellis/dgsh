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

# TODO: replace hard-coded values
NCOMMITTERS=2
FIRST=1356948224
LAST=1474357133
NDAYS=$(( ( $LAST - $FIRST ) / 60 / 60  / 24))

# Commit history in the form of ascending Unix timestamps, emails
git log --pretty=tformat:'%at %ae' |
# Filter records according to timestamp: keep (100000, now) seconds
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

		# Calculate last commit timestamp in seconds
		tail -1 |
		awk '{print $1}' |
		sgsh-writeval -s last &

		# Calculate first commit timestamp in seconds
		head -1 |
		awk '{print $1}' |
		sgsh-writeval -s first &
	}} &

	# Place committers left/right of the median
	# according to the number of their commits
	awk '{print $2}' |
	sort |
	uniq -c |
	sort -n |
	awk 'BEGIN {l = 0; r = '$NCOMMITTERS';}
		{print NR % 2 ? l++ : --r, $2}' |
	sort -k2 &	# <left/right, email>

	sort -k2 &	# <timestamp, email>
}} |
# Join committer positions with commit time stamps
# based on committer email
join -j 2 - - |		# <email, left/right, timestamp>
# Order by timestamp
sort -k 3n |
sgsh-tee |
{{
	# Create portable bitmap
	echo 'P1' &
	echo "$NCOMMITTERS $NDAYS" &
	perl -na -e '
	BEGIN { @empty['$NCOMMITTERS' - 1] = 0; @committers = @empty; }
		sub out { print join("", map($_ ? "1" : "0", @committers)), "\n"; }

		$day = int($F[1] / 60 / 60 / 24);
		$pday = $day if (!defined($pday));

		while ($day != $pday) {
			out();
			@committers = @empty;
			$pday++;
		}

		$committers[$F[2]] = 1;

		END { out(); }
		' &
}} |
sgsh-tee |
# Enlarge points into discs through morphological convolution
pgmmorphconv -erode <(
cat <<EOF
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
