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

export SGSH_DOT_DRAW="$(basename $0 .sh).draw"

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
		awk '{print $1}' &

		# Calculate first commit timestamp in seconds
		head -1 |
		awk '{print $1}' &
	}} |
	# Gather last and first commit timestamp
	sgsh-tee |
	# Make one space-delimeted record
	tr '\n' ' ' |
	# Compute the difference in days
	awk '{print int(($1 - $2) / 60 / 60 / 24)}' |
	# Store number of days
	sgsh-writeval -s days &

	sort -k2 &	# <timestamp, email>

	# Place committers left/right of the median
	# according to the number of their commits
	awk '{print $2}' |
	sort |
	uniq -c |
	sort -n |
	awk '
		BEGIN {
			"sgsh-readval -l -x -s committers" | getline NCOMMITTERS
			l = 0; r = NCOMMITTERS;}
		{print NR % 2 ? l++ : --r, $2}' |
	sort -k2 &	# <left/right, email>

}} |
# Join committer positions with commit time stamps
# based on committer email
join -j 2 - - |		# <email, timestamp, left/right>
# Order by timestamp
sort -k 2n |
sgsh-tee |
{{
	# Create portable bitmap
	echo 'P1' &

	{{
		sgsh-readval -l -s committers &
		sgsh-readval -l -q -s days &
	}} |
	sgsh-tee |
	tr '\n' ' ' |
	awk '{print $1, $2}' &

	perl -na -e '
	BEGIN { open(my $ncf, "-|", "sgsh-readval -l -x -s committers");
		$ncommitters = <$ncf>;
		@empty[$ncommitters - 1] = 0; @committers = @empty; }
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
1 1 1 0 1 1 1
1 1 0 0 0 1 1
1 0 0 0 0 0 1
0 0 0 0 0 0 0
1 0 0 0 0 0 1
1 1 0 0 0 1 1
1 1 1 0 1 1 1
EOF
) |
sgsh-tee |
{{
	# Full-scale image
	pnmtopng >large.png &
	# A smaller image
	pamscale -width 640 |
	pnmtopng >small.png &
}}
