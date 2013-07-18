#!/usr/bin/perl
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

use strict;
use warnings;

# Reproducable results
srand(0);

my $x = $ARGV[0];
shift;
my @host;
my @request;

while (<>) {
	print;

	my ($host, $time, $request, $rest) = ($_ =~ m/^([^\s]+)(\s+[^"]+)(\"[^"]*\")(.*)$/);
	$host[int(rand() * 1000)] = $host;
	$request[int(rand() * 1000)] = $request;
        for (my $i = 0; $i < $x - 1; $i++) {
		my $hpos = int(rand() * 1000);
		my $nhost;
		$host = $nhost if ($nhost = $host[$hpos]);
		my $ppos = int(rand() * 1000);
		my $nrequest;
		$request = $nrequest if ($nrequest = $request[$ppos]);
		print $host, $time, $request, $rest, "\n";
	}
}
