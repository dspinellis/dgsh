#!/usr/bin/perl
#
# Given as an argument a process id, print on the standard output
# performance details of the process's children.
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

# Set to 1 to enable debug print messages
my $debug = 1;

if ($#ARGV != 0 || $ARGV[0] !~ m/^\d+$/) {
	print STDERR "Usage: sgsh-ps process-id\n";
	exit 1;
}

my $qpid = $ARGV[0];

# Operating system name
my $uname = `uname`;
chop $uname;

my $perf;

if ($uname =~ m/cygwin/i) {
	my $q = proclist_win();
	print "query=$q\n" if ($debug);
	$perf = procperf_win($q);
} elsif ($uname eq 'Linux') {
	my $q = proclist_unix();
	print "query=$q\n" if ($debug);
	$perf = procperf_unix(qq{--ppid $qpid -o '%cpu,etime,cputime,psr,%mem,rss,vsz,state,maj_flt,min_flt,args'});
} elsif ($uname eq 'FreeBSD') {
	my $q = proclist_unix();
	print "query=$q\n" if ($debug);
	$perf = procperf_unix(qq{-p $q -ww -o '%cpu,etime,usertime,systime,%mem,rss,vsz,state,majflt,minflt,args'});
} else {
	print STDERR "Unsupported OS [$uname] for debugging info.\n";
	print STDERR "Please submit a patch with the appropriate ps arguments.\n";
	exit 1;
}


print encode_json($perf);

# Create the list of child processes to examine
# in a form suitable for passing to WMIC PROCESS WHERE (...) GET.
# We need this because Cygwin doesn't offer a --ppid ... option.
sub
proclist_win
{
	open(my $ps, '-|', 'ps -l') || die "Unable to run ps: $!\n";
	my $list = '';
	my $sep = '';
	my $header = <$ps>;
	my %children;
	while (<$ps>) {
		my ($pid, $ppid, $pgid, $winpid) = split;
		$children{$ppid} .= ($children{$ppid} ? ' OR ' : '') . "ProcessId=$winpid";
	}
	close $ps;
	return $children{$qpid};
}

# Create the list of child processes to examine
# in a form suitable for passing to ps -p
# We need this because BSD ps doesn't offer a --ppid ... option.
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
		$children{$ppid} .= ($children{$ppid} ? ',' : '') . "$pid";
	}
	close $ps;
	return $children{$qpid};
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
	open(my $wmic, '-|', "WMIC PROCESS WHERE ($q) GET WorkingSetSize,VirtualSize,PageFaults,CreationDate,KernelModeTime,UserModeTime,CommandLine") || die "Unable to run wmic: $!\n";

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

	print "Column positions=", join(',', @colpos), "\n" if ($debug);

	# Get output and form result
	my @result;
	while(<$wmic>) {
		my @procperf;
		last if (/^\s*$/);
		print "read: [$_]\n" if ($debug);
		my %perf;
		for (my $i = 0; $i < $#colpos - 1; $i++) {
			my $value = substr($_, $colpos[$i], $colpos[$i + 1] - $colpos[$i]);
			$value =~ s/\s*$//;
			$perf{$colname[$i]} = $value;
			print "$colname[$i] = [$value]\n" if ($debug);
		}

		# Convert times to seconds
		$perf{'KernelModeTime'} /= 1e7;
		$perf{'UserModeTime'} /= 1e7;

		# Calculate elapsed time and CPU %
		my ($year, $mon, $mday, $hour, $min, $sec, $creation_fraction) = ($perf{'CreationDate'} =~ m/^(\d{4})(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)\.(\d+)/);
		$mon--;
		my $creation_time = timelocal($sec, $min, $hour, $mday, $mon, $year);
		;
		my ($now_sec, $now_usec) = gettimeofday();
		$perf{'ElapsedTime'} = ($now_sec - $creation_time) + ($now_usec / 1e6 - "0.$creation_fraction");
		$perf{'CPU %'} = sprintf('%.2f', ($perf{'KernelModeTime'} + $perf{'UserModeTime'}) / $perf{'ElapsedTime'} * 100);
		delete $perf{'CreationDate'};

		# Format times
		$perf{'KernelModeTime'} = human_time($perf{'KernelModeTime'});
		$perf{'UserModeTime'} = human_time($perf{'UserModeTime'});
		$perf{'ElapsedTime'} = human_time($perf{'ElapsedTime'});

		# Format memory values
		$perf{'WorkingSetSize'} = human_memory($perf{'WorkingSetSize'});
		$perf{'VirtualSize'} = human_memory($perf{'VirtualSize'});
		$perf{'PageFaults'} = human_integer($perf{'PageFaults'});

		# Remove path from command line
		$perf{'CommandLine'} =~ s/^[^"][^ ]*\\//;
		$perf{'CommandLine'} =~ s/^\"[^"]*\\([^"]+)\"/$1/;

		push(@result, \%perf);
	}
	close $wmic;
	return \@result;
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

	open(my $ps, '-|', "ps $q") || die "Unable to run ps: $!\n";

	# The output is fixed column width; obtain column indices and names
	my $header = <$ps>;
	my @fields = split(/\s+/, $header);

	# Get output and form result
	my @result;
	while(<$ps>) {
		my @values = split;
		print "read: [$_]\n" if ($debug);
		my %perf;
		for (my $i = 0; $i <= $#fields; $i++) {
			my $value;
			if ($i == $#fields) {
				$value = join(' ', @values[$i..$#values]);
			} else {
				$value = $values[$i];
			}
			$perf{$fields[$i]} = $value;
			print "$fields[$i] = [$value]\n" if ($debug);
		}

		# Format values
		$perf{'RSS'} = human_memory($perf{'RSS'} * 1024);
		$perf{'VSZ'} = human_memory($perf{'VSZ'} * 1024);
		$perf{'MAJFL'} = human_integer($perf{'MAJFL'}) if (defined($perf{'MAJFL'}));
		$perf{'MINFL'} = human_integer($perf{'MINFL'}) if (defined($perf{'MINFL'}));
		$perf{'MAJFLT'} = human_integer($perf{'MAJFLT'}) if (defined($perf{'MAJFLT'}));
		$perf{'MINFLT'} = human_integer($perf{'MINFLT'}) if (defined($perf{'MINFLT'}));

		push(@result, \%perf);
	}
	close $ps;
	return \@result;
}
