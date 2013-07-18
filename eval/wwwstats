#!/usr/bin/perl
#
#  Copyright 1995-2013 Diomidis Spinellis
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

use Time::Local;

# LogFormat "%h %l %u %t \"%r\" %s %b
# %...h:  remote host
# %...l:  remote logname (from identd, if supplied)
# %...u:  remote user (from auth; may be bogus if return status (%s) is 401)
# %...t:  time, in common log format time format
# %...r:  first line of request
# %...s:  status.  For requests that got internally redirected, this
#         is status of the *original* request --- %...>s for the last.
# %...b:  bytes sent.
# %...{Foobar}i:  The contents of Foobar: header line(s) in the request
#                 sent to the client.
# %...{Foobar}o:  The contents of Foobar: header line(s) in the reply.
#

$monthcard{'Jan'} = '01';
$monthcard{'Feb'} = '02';
$monthcard{'Mar'} = '03';
$monthcard{'Apr'} = '04';
$monthcard{'May'} = '05';
$monthcard{'Jun'} = '06';
$monthcard{'Jul'} = '07';
$monthcard{'Aug'} = '08';
$monthcard{'Sep'} = '09';
$monthcard{'Oct'} = '10';
$monthcard{'Nov'} = '11';
$monthcard{'Dec'} = '12';

$downame{1} = 'Mon';
$downame{2} = 'Tue';
$downame{3} = 'Wed';
$downame{4} = 'Thu';
$downame{5} = 'Fri';
$downame{6} = 'Sat';
$downame{0} = 'Sun';

while (<>) {
	$logbytecount += length($_);
	chop;
	if (!(
# ifweb.dimi.uniud.it - - [11/Mar/2002:22:31:30 +0200] "GET /pubs/conf/1999-ESREL-SoftRel/html/chal.html HTTP/1.1" 404 322
		($host,
		$logname, $user,
		$day, $month, $year,
		$hour, $minute,
		$verb, $url,
		$status, $bytes) = /
		([-\w.]+)\s+	(?# Host)
		([-\w]+)\s+	(?# Logname)
		([-\w]+)\s+	(?# User)
		\[(\d+)\/	(?# Date)
		(\w+)\/		(?# Month)
		(\d+)\:		(?# Year)
		(\d+)\:		(?# Hour)
		(\d+)		(?# Minute)
		[^]]+?\]\s+	(?# Rest of time)
		\"([-\w]+)\s*	(?# Request verb)
		([^\s]*)	(?# Request URL)
		[^"]*?\"\s+	(?# Request protocol etc.)
		(\d+)\s+	(?# Status)
		([-\d]+)	(?# Bytes)
		/x)) {
			print STDERR "$ARGV($.): Unable to process: $_\n";
			next;
	}

	if ($host !~ m/\.\d+$/) {
		($topdomain) = ($host =~ m/.*\.(.*)/);
		$topdomaincount{$topdomain}++;
		($domain) = ($host =~ m/[^.]\.(.*)/);
		$domaincount{$domain}++;
	}

	($area) = ($url =~ m/^\/?([^\/]+)/);
	$area = '/' if ($area eq '');

	$month = $monthcard{$month};
	$date = $year . '-' . $month . '-' . $day;

	$accesscount++;
	$hostcount{$host}++;
	$urlcount{$url}++;
	$datecount{$date}++;
	$ltime = timelocal(0, 0, 0, $day, $month - 1, $year - 1900);
	$daynum = (localtime $ltime)[6];
	$dowcount{$daynum}++;
	$hourcount{$hour}++;
	$areacount{$area}++;
	$bytecount += $bytes;
	$hostbytecount{$host} += $bytes;

}

print "
			WWW server statistics
			=====================

Summary
-------
";
printf("Number of accesses: %d\n", $accesscount);
printf("Number of Gbytes transferred: %d\n", $bytecount / 1024 / 1024 / 1024);
printf("Number of hosts: %d\n", ($hostcount = grep 1, values %hostcount));
printf("Number of domains: %d\n", ($domaincount = grep 1, values %domaincount));
printf("Number of top level domains: %d\n", ($topdomaincount = grep 1, values %topdomaincount));
printf("Number of different pages: %d\n", ($urlcount = grep 1, keys %urlcount));
printf("Accesses per day: %d\n", $accesscount / ($datecount = grep 1, values %datecount));
printf("Mbytes per day: %d\n", $bytecount / $datecount / 1024 / 1024);
printf("Mbytes log file size: %d\n", $logbytecount / 1024 / 1024);


print '
Top 20 Requests
---------------
';
$count = 0;
foreach (sort {$urlcount{$b} <=> $urlcount{$a}} keys %urlcount) {
	last if ($count++ == 20);
	printf("%10d %s\n", $urlcount{$_}, $_);
}

print '
Top 20 Area Requests
---------------
';
$count = 0;
foreach (sort {$areacount{$b} <=> $areacount{$a}} keys %areacount) {
	last if ($count++ == 20);
	printf("%10d %s\n", $areacount{$_}, $_);
}

print '
Top 10 Hosts
------------
';
$count = 0;
foreach (sort {$hostcount{$b} <=> $hostcount{$a}} keys %hostcount) {
	last if ($count++ == 10);
	printf("%10d %s\n", $hostcount{$_}, $_);
}


print '
Top 10 Hosts by Transfer
------------------------
';
$count = 0;
foreach (sort {$hostbytecount{$b} <=> $hostbytecount{$a}} keys %hostbytecount) {
	last if ($count++ == 10);
	printf("%10d %s\n", $hostbytecount{$_}, $_);
}


print '
Top 10 Domains
--------------
';
$count = 0;
foreach (sort {$domaincount{$b} <=> $domaincount{$a}} keys %domaincount) {
	last if ($count++ == 10);
	printf("%10d %s\n", $domaincount{$_}, $_);
}

print '
Top 20 Level Domain Accesses
-------------------------
';
$count = 0;
foreach (sort {$topdomaincount{$b} <=> $topdomaincount{$a}} keys %topdomaincount) {
	printf("%10d %s\n", $topdomaincount{$_}, $_);
	last if ($count++ == 20);
}

print '
Accesses by Day of Week
-----------------------
';
foreach (sort {$dowcount{$b} <=> $dowcount{$a}} keys %dowcount) {
	printf("%10d %s\n", $dowcount{$_}, $downame{$_});
}

print '
Accesses by Local Hour
----------------------
';
foreach (sort keys %hourcount) {
	printf("%10d %s\n", $hourcount{$_}, $_);
}

print '
Accesses by Date
----------------
';
foreach (sort keys %datecount) {
	printf("%10d %s\n", $datecount{$_}, $_);
}
