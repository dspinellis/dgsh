#!/usr/bin/awk -f
#
# Grow the size of a web server log file by a factor of N,
# which is specified as the first argument, by repeating each
# line N times.  The host name and the request are changed by
# substituting them with a random pick form a list of 1000 entries.
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

BEGIN { x = ARGV[1]; ARGC-- }
{
	print;
	host[int(rand() * 1000)] = $1;
	page[int(rand() * 1000)] = $7;
        for (i = 0; i < x - 1; i++) {
		hpos = int(rand() * 1000);
		if (nhost = host[hpos])
			$1 = nhost;
		ppos = int(rand() * 1000);
		if (npage = page[ppos])
			$7 = npage;
		print;
	}
}

