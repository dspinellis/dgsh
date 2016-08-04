#
# SYNOPSIS Git commit statistics
# DESCRIPTION
# Process the git history, and list the authors and days of the week
# ordered by the number of their commits.
# Demonstrates streams and piping through a function.
#
#  Copyright 2012-2013 Diomidis Spinellis
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

sgsh-wrap git log --format="%an:%ad" --date=default "$@" |
sgsh-tee |
{{
	sgsh-wrap echo "Authors ordered by number of commits" &
	# Order by frequency
	sgsh-wrap awk -F: '{print $1}' | sort | sgsh-wrap uniq -c | sort -rn &

	sgsh-wrap echo "Days ordered by number of commits" &
	# Order by frequency
	sgsh-wrap awk -F: '{print substr($2, 1, 3)}' | sort | sgsh-wrap uniq -c | sort -rn &
}} |
sgsh-tee
