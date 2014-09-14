#!/usr/bin/env perl
#
# Merge sorted (value, key) pairs, summing the values of equal keys
#
#  Copyright 2014 Diomidis Spinellis
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

use Unicode::Collate::Locale;

my $Collator = Unicode::Collate->new(locale => $ENV{'LANG'});

# Read a record from the specified file reference
sub
read_record
{
	my ($fr) = @_;
	my $f = $fr->{file};
	my $line = <$f>;
	if (!defined($line)) {
		$fr->{key} = undef;
		return;
	}
	($fr->{value}, $fr->{key}) = ($line =~ m/^\s*(\d+)\s+(.*)/);
}

# Open input files; opening before reading prevents pipe writers from blocking
my @file;
my $i = 0;
for my $name (@ARGV) {
	open($file[$i]->{file}, '<:encoding(utf8)', $name) || die "Unable to open $name: $!\n";
	$i++;
}

# Read first record from all files
for my $f (@file) {
	read_record($f);
}

# Previous key printed
my $prev;

for (;;) {
	# Find smallest key
	my $smallest;
	for my $r (@file) {
		#print "Check $r->{value}, $r->{key}\n";
		$smallest = $r if (!defined($smallest->{key}) ||
			(defined($r->{key}) && $r->{key} lt $smallest->{key}));
	}

	exit 0 unless defined($smallest->{key});
	#print "Smallest $smallest->{value}, $smallest->{key}\n";

	# Sum up and renew all smallest keys
	my $sum = 0;
	my $key = $smallest->{key};
	for my $r (@file) {
		if (defined($r->{key}) && $Collator->cmp($r->{key}, $key) == 0) {
			$sum += $r->{value};
			read_record($r);
		}
	}

	# Verify that input is sorted
	if (defined($prev) && $Collator->cmp($key, $prev) < 0) {
		print STDERR "Input is not sorted: [$key] came after [$prev]\n";
		exit 1;
	}
	$prev = $key;

	print "$sum $key\n";
}
