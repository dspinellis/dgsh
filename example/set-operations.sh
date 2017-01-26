#!/usr/bin/env dgsh
#
# SYNOPSIS Set operations
# DESCRIPTION
# Combine, update, aggregate, summarise results files, such as logs.
# Demonstrates combined use of tools adapted for use with dgsh:
# sort, comm, paste, join, and diff.
#
#  Copyright 2016 Marios Fragkoulis
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

if [ -z "$1" ]; then
	PSDIR=simple-shell
else
	PSDIR=$1
fi

cp $PSDIR/results $PSDIR/res

# Sort result files
{{
	sort $PSDIR/f4s &
	sort $PSDIR/f5s &
}} |
# Remove noise
comm |
{{
	# Paste to master results file
	paste $PSDIR/res > results &

	# Join with selected records
	join $PSDIR/top > top_results &

	# Diff from previous results file
	diff $PSDIR/last > diff_last &
}}
