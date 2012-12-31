#!/usr/bin/perl
#
# Read as input a shell script extended with scatter-gather operation syntax
# Produce as output a plain shell script implementing the specified operations
# through named pipes
#

use strict;
use warnings;

open(my $in, '<', $ARGV[0]) || die "Unable to open $ARGV[0]: $!\n";
my @lines = <$in>;

# The scatter point currently in effect
my $current_point;

# For assigning unique scatter point ids
my $point_counter;

# Used for saving/restoring current_point
my @current_point_stack;

# Number of endpoints for each scatter point
my @endpoint_number;

# True when processing a scatter gather block
my $in_scatter_gather_block = 0;

# Number of gather input points
my $gather_points;

# Line where S/G block starts
my $scatter_gather_start;

# Variable name for each gather endpoint
my @gather_name;

for (my $i = 0; $i <= $#lines; $i++) {
	$_ = $lines[$i];
	# Scatter block begin
	if (/scatter\s*\|\{/) {
		if ($in_scatter_gather_block) {
			print STDERR "$ARGV[0](", $i + 1, "): Scatter-gather blocks can't be nested\n";
			exit 1;
		}
		$point_counter = -1;
		$gather_points = 0;
		$scatter_gather_start = $i;
		$in_scatter_gather_block = 1;
		undef @endpoint_number;
		undef @current_point_stack;
		undef @gather_name;
		next;

	# Gather block begin
	} elsif (/\|\}\s*gather\s*\|\{/) {
		generate_scatter_code($scatter_gather_start, $i - 1);
		$i += generate_gather_code($i);
		$in_scatter_gather_block = 0;
		next;

	# Scatter group end
	} elsif (/\|\}/) {
		if ($#current_point_stack == -1) {
			print STDERR "$ARGV[0](", $i + 1, "): Extra |}\n";
			exit 1;
		}
		$current_point = pop(@current_point_stack);
		next;
	}

	# Scatter input endpoint
	if (/-\|/) {
		$endpoint_number[$current_point]++;
	}

	# Scatter group begin
	if (/\|\{/) {
		push(@current_point_stack, $current_point);
		$current_point = ++$point_counter;
	}

	# Gather output endpoint
	if (/\|\=\s*(\w+)/) {
		$gather_name[$gather_points++] = $1;
	}

	# Print the line, unless we're in a scatter-gather block
	print unless ($in_scatter_gather_block);
}

#
# Generate the code to scatter data
# Arguments are the beginning and end lines of the corresponding scatter block
# Uses the global variables: @lines, $point_counts, @endpoint_number, $gather_points
#
sub
generate_scatter_code
{
	my($start, $end) = @_;
	# The scatter point currently in effect
	my $current_point;

	# For assigning unique scatter point ids
	my $point_counter = -1;

	# Used for saving/restoring current_point
	my @current_point_stack;

	# Count endpoints for each scatter point
	my @endpoint_counter;

	for (my $i = $start; $i <= $end; $i++) {
		$_ = $lines[$i];
		# Scatter block begin: initialize named pipes
		if (/scatter\s*\|\{/) {
			# Generate initialization code
			my $code = 'export SGDIR=/tmp/sg-$$; rm -rf $SGDIR; mkdir $SGDIR; mkfifo';
			# Scatter named pipes
			for (my $j = 0; $j <= $#endpoint_number; $j++) {
				for (my $k = 0; $k < $endpoint_number[$j]; $k++) {
					$code .= " \$SGDIR/npi-$j.$k";
				}
			}
			# Gather named pipes
			for (my $j = 0; $j < $gather_points; $j++) {
				$code .= " \$SGDIR/npo-$gather_name[$j]";
			}
			s/scatter\s*\|\{/$code/;

		# Gather group begin
		} elsif (/\|\}\s*gather\s*\|\{/) {
			generate_scatter_code($scatter_gather_start, $i - 1);
			$i += generate_gather_code($i);

		# Scatter group end: maintain stack
		} elsif (/\|\}/) {
			$current_point = pop(@current_point_stack);
			s/\|\}//;

		}

		# Scatter input head endpoint: get input from named pipe
		if (/-\|/) {
			s/-\|/<\$SGDIR\/npi-$current_point.$endpoint_counter[$current_point]/;
			$endpoint_counter[$current_point]++;
		}

		# Scatter group begin: tee output to named pipes
		if (/\|\{/) {
			push(@current_point_stack, $current_point) if (defined($current_point));
			$current_point = ++$point_counter;
			my $tee_args;
			my $j;
			for ($j = 0; $j  < $endpoint_number[$current_point] - 1; $j++) {
				$tee_args .= " \$SGDIR/npi-$current_point.$j";
			}
			$tee_args .= " >\$SGDIR/npi-$current_point.$j";
			s/\|\{/| tee $tee_args &/;
			$endpoint_counter[$current_point] = 0;
		}

		# Gather output endpoint
		if (/\|\=\s*(\w+)/) {
			s/\|\=\s*(\w+)/>\$SGDIR\/npo-$1 &/;
		}

		print;
	}
}

# Generate gather code for the gather block starting in the passed line
# Return the number of lines in the block
sub
generate_gather_code
{
	my($start) = @_;

	my $i;
	for ($i = $start; $i <= $#lines; $i++) {
			$_ = $lines[$i];
		if (/\|\}\s*gather\s*\|\{/) {
			s/\|\}\s*gather\s*\|\{//;
			print "# Gather the results\n(\n";
			for (my $j = 0; $j <= $#gather_name; $j++) {
				print qq{\techo "$gather_name[$j]='`cat \$SGDIR/npo-$gather_name[$j]`'" &\n};
			}
			print "\twait\nrm -rf \$SGDIR\ncat <<\\SGEOFSG\n";
		} elsif (/\|\}/) {
			s/\|\}//;
			my $shell = $lines[0];
			if (!($shell =~ s/^\#\!//)) {
				print STDERR "ARGV[0](1): No shell specified\n";
				exit 1;
			}
			$shell =~ s/\s.*$//;
			print "SGEOFSG\n) | $shell\n";
			last;
		} else {
			print $lines[$i];
		}
	}
	return $i;
}
