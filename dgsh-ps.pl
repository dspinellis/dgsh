#!/usr/bin/perl
#
# Given as an argument a process id, print on the standard output
# performance details of the process's descendants.
# Handles formatting and variations between POSIX-like operating environments,
# including Cygwin.
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
use JSON;

# Set to >0 to enable debug print messages
my $debug = 0;

# Map from Cygwin to Windows process ids
my %winpid;

# Number of process entries to return
my $max_entries = 2;

if ($#ARGV != 0 || $ARGV[0] !~ m/^\d+$/) {
	print STDERR "Usage: dgsh-ps process-id\n";
	exit 1;
}

my $qpid = $ARGV[0];

print STDERR "qpid=$qpid\n" if ($debug);

# Operating system name
my $uname = `uname`;
chop $uname;

my $perf;

if ($uname =~ m/cygwin/i) {
	my $q = proclist_win();
	print STDERR "query=$q\n" if ($debug);

	exit_if_empty($q);
	$perf = procperf_win($q);
} elsif ($uname eq 'Linux') {
	my $q = proclist_unix();

	exit_if_empty($q);
	$perf = procperf_unix(qq{-p $q -ww -o '%cpu,etime,cputime,psr,%mem,rss,vsz,state,maj_flt,min_flt,args'});
} elsif ($uname eq 'FreeBSD') {
	my $q = proclist_unix();

	exit_if_empty($q);
	$perf = procperf_unix(qq{-p $q -ww -o '%cpu,etime,usertime,systime,%mem,rss,vsz,state,majflt,minflt,args'});
} elsif ($uname eq 'Darwin') {		# Mac OS X
	my $q = proclist_unix();

	exit_if_empty($q);
	$perf = procperf_unix(qq{-p $q -ww -o '%cpu,etime,%mem,rss,vsz,state,command'});
} else {
	print STDERR "Unsupported OS [$uname] for debugging info.\n";
	print STDERR "Please submit a patch with the appropriate ps arguments.\n";
	exit 1;
}

# Obtain the top-two entries
$perf = [(sort compare @$perf)[0..($max_entries - 1)]];

print encode_json($perf), "\n";

# Create the list of child processes to examine in a form suitable for passing to
# WMIC PROCESS WHERE (...) GET.
# We need this in order to calculate a transitive closure of the parent-child relationship
# As a side effect set %winpid with a map from Cygwin to Windows process ids
sub
proclist_win
{
	open(my $ps, '-|', 'ps -l') || die "Unable to run ps: $!\n";
	my $list = '';
	my $sep = '';
	my $header = <$ps>;
	my %children;
	while (<$ps>) {
		print STDERR "ps $_" if ($debug > 1);
		my ($pid, $ppid, $pgid, $winpid) = split;
		push(@{$children{$ppid}}, $pid);
		$winpid{$pid} = $winpid;
	}
	close $ps;
	my @closure = transitive_closure($qpid, \%children);
	print STDERR "Closure: ", join(',', @closure), "\n" if ($debug);
	return combinelist_win(@closure);
}

# Given a reference to a map from parent process id to an array of children process ids
# return the transitive closure starting from process p
sub
transitive_closure
{
	my($start, $tree) = @_;
	my @result;

	print STDERR "Transitive closure starting from $start\n" if ($debug > 0);
	#for keys ", keys(%$tree), " values ", values(%$tree), "\n" if ($debug > 3);
	return () unless defined($tree->{$start});
	for my $p (@{$tree->{$start}}) {
		push(@result, $p);
		push(@result, transitive_closure($p, $tree));
	}
	print STDERR "Returning for $start ", join(',', @result), "\n" if ($debug > 0);
	return @result;
}

# Combine a list of process ids into a form suitable for passing to
# WMIC PROCESS WHERE (...) GET.
sub
combinelist_win
{
	my $result = '';

	for my $p (@_) {
		$result .= ($result ? ' OR ' : '') . "ProcessId=$winpid{$p}";
	}
	return $result;
}

# Combine a list of process ids into a form suitable for passing to ps(1)
sub
combinelist_unix
{
	my $result = '';

	for my $p (@_) {
		$result .= ($result ? ',' : '') . $p;
	}
	return $result;
}

# Create the list of child processes to examine
# in a form suitable for passing to ps -p
# We need this in order to calculate a transitive closure of the parent-child relationship
sub
proclist_unix
{
	open(my $ps, '-|', 'ps -o pid,ppid') || die "Unable to run ps: $!\n";
	my $list = '';
	my $sep = '';
	my $header = <$ps>;
	my %children;
	while (<$ps>) {
		my ($pid, $ppid) = split;
		push(@{$children{$ppid}}, $pid);
	}
	close $ps;
	my @closure = transitive_closure($qpid, \%children);
	print STDERR "Closure: ", join(',', @closure), "\n" if ($debug);
	return combinelist_unix(@closure);
}

# Return a human-reable version of a memory figure expressed in bytes
sub
human_memory
{
	my ($m) = @_;

	if ($m > 1024 * 1024 * 1024) {
		$m = sprintf('%.1fGiB', $m / 1024 / 1024 / 1024);
	} elsif ($m > 1024 * 1024) {
		$m = sprintf('%.1fMiB', $m / 1024 / 1024);
	} elsif ($m > 1024) {
		$m = sprintf('%.1fKiB', $m / 1024);
	} else {
		$m = "${m}B";
	}
	# Remove trailing decimals if enough significant digits
	$m =~ s/\.\d+// if (length($m) > 7);
	return $m;
}

# Return a human-reable version of a time figure expressed in seconds
sub
human_time
{
	my ($sec) = @_;

	my $min = int($sec / 60);
	if ($min) {
		$sec -= $min * 60;
		$sec = sprintf('%.0f', $sec);
		my $hour = int($min / 60);
		if ($hour) {
			$min -= $hour * 60;
			return sprintf('%d:%02d:%02d', $hour, $min, $sec);
		}
		return sprintf('%02d:%0d', $min, $sec);
	}
	return sprintf('%.2f', $sec);
}

# Convert an integer number to a human-readable form
# by adding thousand separators
sub
human_integer
{
	my ($n) = @_;
	$n =~ s/\B(?=(\d{3})+(?!\d))/,/g;
	return $n;
}

# Given a query for processes to list, return an array of references
# to hashes containing key-value pairs of performance figures
# Windows (Cygwin) version
sub
procperf_win
{
	use Time::Local;
	use Time::HiRes 'gettimeofday';
	my ($q) = @_;

	# For all the allowed fields run Google Win32_Process class
	open(my $wmic, '-|', "WMIC PROCESS WHERE '($q)' GET WorkingSetSize,VirtualSize,PageFaults,CreationDate,KernelModeTime,UserModeTime,CommandLine") || die "Unable to run wmic: $!\n";

	# The output is fixed column width; obtain column indices and names
	my $header = <$wmic>;
	my @colpos;
	my @colname;
	push(@colpos, 0);
	while ($header =~ s/(\w+\s+)//) {
		my $key = $1;
		push(@colpos, $colpos[$#colpos] + length($key));
		$key =~ s/\s+$//;
		push(@colname, $key);
	}
	push(@colpos, -1);
	$header =~ s/\s*$//;
	push(@colname, $header);

	print STDERR "Column positions=", join(',', @colpos), "\n" if ($debug);

	# Get output and form result
	my @result;
	while(<$wmic>) {
		my @procperf;
		last if (/^\s*$/);
		print STDERR "read: [$_]\n" if ($debug);
		my %perf;
		for (my $i = 0; $i < $#colpos - 1; $i++) {
			my $value = substr($_, $colpos[$i], $colpos[$i + 1] - $colpos[$i]);
			$value =~ s/\s*$//;
			$perf{$colname[$i]} = $value;
			print STDERR "$colname[$i] = [$value]\n" if ($debug);
		}

		# Convert times to seconds
		$perf{'KernelModeTime'} /= 1e7;
		$perf{'UserModeTime'} /= 1e7;

		# Calculate elapsed time used for CPU %
		my ($year, $mon, $mday, $hour, $min, $sec, $creation_fraction) = ($perf{'CreationDate'} =~ m/^(\d{4})(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)\.(\d+)/);
		$mon--;
		my $creation_time = timelocal($sec, $min, $hour, $mday, $mon, $year);
		my ($now_sec, $now_usec) = gettimeofday();
		my $elapsed = ($now_sec - $creation_time) + ($now_usec / 1e6 - "0.$creation_fraction");

		# Remove path from command line
		$perf{'CommandLine'} =~ s/^[^"][^ ]*\\//;
		$perf{'CommandLine'} =~ s/^\"[^"]*\\([^"]+)\"/$1/;

		# Store the results in the order required for presentation
		my $entry = {
			'sortKey' => $perf{KernelModeTime} + $perf{UserModeTime},
			'command' => $perf{'CommandLine'},
			'kv' => [
				{ 'k' => 'CPU %',	'v' => sprintf('%.2f', ($perf{'KernelModeTime'} + $perf{'UserModeTime'}) / $elapsed * 100) },
				{ 'k' => 'Elapsed time','v' => human_time($elapsed) },
				{ 'k' => 'User time',	'v' => human_time($perf{'UserModeTime'}) },
				{ 'k' => 'System time',	'v' => human_time($perf{'KernelModeTime'}) },
				{ 'k' => 'RSS',		'v' => human_memory($perf{'WorkingSetSize'}) },
				{ 'k' => 'VSZ',		'v' => human_memory($perf{'VirtualSize'}) },
				{ 'k' => 'Major faults','v' => human_integer($perf{'PageFaults'}) },
			]
		};
		push(@result, $entry);
	}
	close $wmic;
	return \@result;
}

# Given two to hashes containing key-value pairs of Windows performance figures
# compare the by the sum of the KernelModeTime and UserModeTime
sub
compare
{
	return  $b->{sortKey} <=> $a->{sortKey};
}

# Given a query for processes to list, return an array of references
# to hashes containing key-value pairs of performance figures
# Unix version
# Argument is the argument passed to ps
# The last column is reserved for the command and can contain spaces
sub
procperf_unix
{
	my ($q) = @_;

	print STDERR "query=$q\n" if ($debug);
	open(my $ps, '-|', "ps $q") || die "Unable to run ps: $!\n";

	# The output is fixed column width; obtain column indices and names
	my $header = <$ps>;
	my @fields = split(/\s+/, $header);

	# Get output and form result
	my @result;
	while(<$ps>) {
		my @values = split;
		print STDERR "read: [$_]\n" if ($debug);
		my %perf;
		for (my $i = 0; $i <= $#fields; $i++) {
			my $value;
			if ($i == $#fields) {
				$value = join(' ', @values[$i..$#values]);
			} else {
				$value = $values[$i];
			}
			$perf{$fields[$i]} = $value;
			print STDERR "$fields[$i] = [$value]\n" if ($debug);
		}

		# Store the results in the order required for presentation
		# These are the fields of each OS.
		# Darwin: %CPU ELAPSED %MEM    RSS      VSZ STAT COMMAND
		# FreeBSD: %CPU      ELAPSED  USERTIME   SYSTIME %MEM   RSS   VSZ STAT MAJFLT MINFLT COMMAND
		# Linux: %CPU     ELAPSED     TIME PSR %MEM   RSS    VSZ S  MAJFL  MINFL COMMAND

		# Common entries
		my $entry = {
			'sortKey' => $perf{'%CPU'},
			'command' => $perf{'COMMAND'},
			'kv' => [
				# Initial common entries
				{ 'k' => 'CPU %',	'v' => $perf{'%CPU'} },
				{ 'k' => 'Elapsed time','v' => $perf{'ELAPSED'} },
			]
		};

		# Additional entries, where available
		push(@{$entry->{'kv'}},
			{ 'k' => 'CPU time',	'v' => $perf{'TIME'} })
				if defined($perf{'TIME'});
		push(@{$entry->{'kv'}},
			{ 'k' => 'User time',	'v' => $perf{'USERTIME'} })
				if defined($perf{'USERTIME'});
		push(@{$entry->{'kv'}},
			{ 'k' => 'System time',	'v' => $perf{'SYSTIME'} })
				if defined($perf{'SYSTIME'});
		push(@{$entry->{'kv'}},
			{ 'k' => 'Processor #',	'v' => $perf{'PSR'} })
				if defined($perf{'PSR'});
		push(@{$entry->{'kv'}},
			{ 'k' => 'Memory %',	'v' => $perf{'%MEM'} });
		push(@{$entry->{'kv'}},
			{ 'k' => 'RSS',		'v' => human_memory($perf{'RSS'} * 1024) });
		push(@{$entry->{'kv'}},
			{ 'k' => 'VSZ',		'v' => human_memory($perf{'VSZ'} * 1024) });
		push(@{$entry->{'kv'}},
			{ 'k' => 'State',	'v' => $perf{'S'} })
				if defined($perf{'S'});
		push(@{$entry->{'kv'}},
			{ 'k' => 'State',	'v' => $perf{'STAT'} })
				if defined($perf{'STAT'});
		push(@{$entry->{'kv'}},
			{ 'k' => 'Major faults','v' => human_integer($perf{'MAJFLT'}) })
				if defined($perf{'MAJFLT'});
		push(@{$entry->{'kv'}},
			{ 'k' => 'Major faults','v' => human_integer($perf{'MAJFL'}) })
				if defined($perf{'MAJFL'});
		push(@{$entry->{'kv'}},
			{ 'k' => 'Minor faults','v' => human_integer($perf{'MINFLT'}) })
				if defined($perf{'MINFLT'});
		push(@{$entry->{'kv'}},
			{ 'k' => 'Minor faults','v' => human_integer($perf{'MINFL'}) })
				if defined($perf{'MINFL'});

		push(@result, $entry);
	}
	close $ps;
	return \@result;
}

# Exit with an empty JSON array if the query is empty
# to avoid running ps/WMIC with an invalid argument.
sub
exit_if_empty
{
	my ($q) = @_;

	if ($q eq '') {
		print encode_json([]), "\n";
		exit 0;
	}
}
