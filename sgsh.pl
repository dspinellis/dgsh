#!/usr/bin/perl
#
# Read as input a shell script extended with scatter-gather operation syntax
# Produce as output and execute a plain shell script implementing the
# specified operations through named pipes
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

use strict;
use warnings;
use File::Temp qw/ tempfile /;
use Getopt::Std;

# Command-line options
# -k		Keep temporary file
# -n		Do not run the generated script
# -s shell	Specify shell to use
# -t tee	Path to teebuff

our($opt_s, $opt_t, $opt_k, $opt_n);
$opt_s = '/bin/sh';
$opt_t = 'teebuff';
getopts('kns:t:');

$File::Temp::KEEP_ALL = 1 if ($opt_k);

# Output file
my ($output_fh, $output_filename) = tempfile(UNLINK => 1);

# Map containing all defined gather file names
my %defined_gather_file;

# Map containing all defined gather variable names
my %defined_gather_variable;

# Map from /sgsh/ file names into (possibly multiple) named pipes
my %parallel_file_map;

# Ordinal number of the scatter command being processed
my $global_scatter_n = 0;

# Lines of the input file
# Required, because we implement a double-pass algorithm
my @lines;

my $line_number = 0;

# User-specified input file name (or STDIN)
my $input_filename;

# Regular expressions for the input files scatter-gather operators
# scatter |{
my $SCATTER_BLOCK_BEGIN = q!^[^'#"]*scatter\s*\|\{\s*(.*)?$!;
# |{
my $SCATTER_BEGIN = q!\|\{\s*(.*)?$!;
# |} gather |{
my $GATHER_BLOCK_BEGIN = q!^[^'#"]*\|\}\s*gather\s*\|\{\s*(\#.*)?$!;
# |}
my $BLOCK_END = q!^[^'#"]*\|\}\s*(\#.*)?$!;
# -|
my $SCATTER_INPUT = q!^[^'#"]*-\|!;
# .|
my $NO_INPUT = q!^[^'#"]*\.\|!;
# |= name
my $GATHER_VARIABLE_OUTPUT = q!\|\=\s*(\w+)\s*(\#.*)?$!;
# |>/sgsh/name
my $GATHER_FILE_OUTPUT = q!\|\>\s*\/sgsh\/(\w+)\s*(\#.*)?$!;
# |.
my $NO_OUTPUT = q!\|\.\s*(\#.*)?$!;
# -||>/sgsh/name
my $SCATTER_GATHER_PASS_THROUGH = q!^[^'#"]*-\|\|\>\s*\/sgsh\/(\w+)\s*(\#.*)?$!;
# Line comment lines. Skip them to avoid getting confused by commented-out sgsh lines.
my $COMMENT_LINE = '^\s*(\#.*)?$';


# Read input file
if ($#ARGV >= 0) {
	$input_filename = shift;
	open(my $in, '<', $input_filename) || die "Unable to open $input_filename: $!\n";
	@lines = <$in>;
} else {
	$input_filename = 'STDIN';
	@lines = <STDIN>;
}

print $output_fh "#!$opt_s
# Automatically generated file
# Source file $input_filename
";

# Adjust command interpreter line
$lines[0] =~ s/^\#\!/#/;

my $code = '';
my $pipes = '';

# Parse the file and generate the corresponding code
while (get_next_line()) {
	# Scatter block begin: scatter |{
	if (!/$COMMENT_LINE/o && /$SCATTER_BLOCK_BEGIN/o) {

		# Top-level scatter command
		my %scatter_command;
		$scatter_command{line_number} = $line_number;
		$scatter_command{input} = 'stdin';
		$scatter_command{output} = 'scatter';
		$scatter_command{body} = '';
		$scatter_command{scatter_flags} = $1;
		$scatter_command{scatter_commands} = parse_scatter_command_sequence($GATHER_BLOCK_BEGIN);

		my $gather_commands = parse_gather_command_sequence();

		verify_code(\%scatter_command, $gather_commands);

		my ($code2, $pipes2) = scatter_code_and_pipes(\%scatter_command);
		$code .= $code2;
		$pipes .= $pipes2;
		$code .= gather_code($gather_commands);
	} else {
		$code .= $_ ;
	}
}

# The traps ensure that the named pipe directory
# is removed on termination and that the exit code
# after a signal is that of the shell: 128 + signal number
print $output_fh q{
export SGDIR=/tmp/sg-$$
rm -rf $SGDIR
trap 'rm -rf "$SGDIR"' 0
trap 'exit $?' 1 2 3 15
mkdir $SGDIR
},
qq{
mkfifo $pipes
$code
};

# Execute the shell on the generated file
# -a inherits the shell's variables to subshell
my @args = ($opt_s, '-a', $output_filename, @ARGV);

if ($opt_n) {
	print join(' ', @args), "\n";
	exit 0;
}

system(@args);
if ($? == -1) {
	print STDERR "Unable to execute $opt_s: $!\n";
	exit 1;
} else {
	# Convert Perl's system exit code into one compatible with sh(1)
	exit (($? >> 8) | (($? & 127) << 8));
}

# Parse and return a sequence of scatter commands until we reach the specified
# block end
sub
parse_scatter_command_sequence
{
	my ($block_end) = @_;
	my @commands;

	for (;;) {
		if (!get_next_line()) {
			error('End of file while parsing scatter command sequence');
		}
		if (/$COMMENT_LINE/o) {
			next;
		} elsif (/$block_end/) {
			return \@commands;
		} else {
			unget_line();
			my $cmd = parse_scatter_command();
			push(@commands, $cmd);
		}
	}
}

# Parse and return a single scatter command with the following syntax
# scatter_command = ('.|' | '-|')
#	[body]
#	('|>' filename |
#	 '|=' varname |
#	 '|{' flags scatter_command_sequence |
#	 '|.')
# The return is a hash with the following elements:
# input: 		none|scatter
# body:			Command's text
# output:		none|scatter|file|variable
# scatter_commands:	When output = scatter
# scatter_flags:	When output = scatter
# variable_name:	When output = variable
# file_name:		When output = file
sub
parse_scatter_command
{
	my %command;

	$command{body} = '';

	while (get_next_line()) {
		# Don't look at comment lines
		if (/$COMMENT_LINE/o) {
			$command{body} .= $_;
			next;
		}

		# Scatter input endpoint: -|
		if (s/$SCATTER_INPUT//o) {
			error("Input source already defined") if (defined($command{input}));
			$command{line_number} = $line_number;
			$command{input} = 'scatter';
		}

		# No input : .|
		if (s/$NO_INPUT//o) {
			error("Input source already defined") if (defined($command{input}));
			$command{line_number} = $line_number;
			$command{input} = 'none';
		}

		# Scatter group begin: |{
		if (s/$SCATTER_BEGIN//o) {
			error("Headless command in scatter block") if (!defined($command{input}));
			$command{output} = 'scatter';
			$command{body} .= $_;
			$command{scatter_flags} = $1;
			$command{scatter_commands} = parse_scatter_command_sequence($BLOCK_END);
			return \%command;
		}

		# Gather variable output endpoint: |=
		if (s/$GATHER_VARIABLE_OUTPUT//o) {
			error("Headless command in scatter block") if (!defined($command{input}));
			error("Variable $1 already used") if (defined($defined_gather_variable{$1}));
			$defined_gather_variable{$1} = 1;
			$command{output} = 'variable';
			$command{variable_name} = $1;
			$command{body} .= $_;
			return \%command;
		}

		# Gather file output endpoint: |>
		if (s/$GATHER_FILE_OUTPUT//o) {
			error("Headless command in scatter block") if (!defined($command{input}));
			error("Output file $1 already used") if (defined($defined_gather_file{$1}));
			$defined_gather_file{$1} = 1;
			$parallel_file_map{$1} = '';
			$command{output} = 'file';
			$command{file_name} = $1;
			$command{body} .= $_;
			return \%command;
		}

		# Scatter group end: |}
		if (/$BLOCK_END/o) {
			error("Unterminated scatter command");
		}

		# Append command to body
		$command{body} .= $_;
	}
	error("End of file reached file parsing a scatter command");
}


# Parse a sequence of gather commands and return it as a single scalar
sub
parse_gather_command_sequence
{
	my $command = '';

	while (get_next_line()) {
		# Gather block end: |}
		if (/$BLOCK_END/o) {
			return $command;
		}
		$command .= $_;
	}
	error("End of file reached file parsing a gather block");
}

# Return the code and the named pipes that must be generated
# to scatter data for the specified command
sub
scatter_code_and_pipes
{
	my($command) = @_;
	my $code = '';
	my $pipes = '';

	my $scatter_n = $global_scatter_n++;

	# Parse arguments
	my @save_argv = @ARGV;
	my %scatter_opts;
	@ARGV = split(/\s+/, $command->{scatter_flags});
	getopts('lp:st', \%scatter_opts);
	@ARGV = @save_argv;

	# Number of commands to invoke in parallel
	my $parallel;
	if ($scatter_opts{'p'}) {
		$parallel = $scatter_opts{'p'};
	} else {
		$parallel = 1;
	}

	# Count number of scatter targets;
	my $commands = $command->{scatter_commands};

	my $scatter_targets = 0;
	for my $c (@{$commands}) {
		$scatter_targets++ if ($c->{input} eq 'scatter');
	}

	# Create tee, if needed
	if ($scatter_targets * $parallel > 1) {
		# Create arguments
		my $tee_args;
		$tee_args .= ' -s' if ($scatter_opts{'s'});
		$tee_args .= ' -l' if ($scatter_opts{'l'});
			for (my $cmd_n = 0; $cmd_n  < $scatter_targets; $cmd_n++) {
				for (my $p = 0; $p < $parallel; $p++) {
					$tee_args .= " \$SGDIR/npi-$scatter_n.$cmd_n.$p";
				}
			}
		# Obtain tee program
		my $tee_prog = $opt_t;
		$tee_prog = $scatter_opts{'t'} if ($scatter_opts{'t'});

		$code .= "$tee_prog $tee_args &\n";
	}

	# Process the commands
	my $cmd_n = 0;
	for my $c (@{$commands}) {
		for (my $p = 0; $p < $parallel; $p++) {

			# Pass-through fast exit
			if ($c->{input} eq 'scatter' && $c->{body} eq '' && $c->{output} eq 'file') {
				$code .= "ln -s \$SGDIR\/npi-$scatter_n.$cmd_n.$p \$SGDIR\/npfo-$c->{file_name}.$p\n";
				$parallel_file_map{$c->{file_name}} .= " \$SGDIR\/npfo-$c->{file_name}.$p";
				$pipes .= " \\\n\$SGDIR/npi-$scatter_n.$cmd_n.$p";
				next;
			}

			# Generate input
			if ($c->{input} eq 'none') {
				$code .= '</dev/null ';
			} elsif ($c->{input} eq 'scatter') {
				$code .= " <\$SGDIR/npi-$scatter_n.$cmd_n.$p";
				$pipes .= " \\\n\$SGDIR/npi-$scatter_n.$cmd_n.$p";
			} else {
				die "Headless command";
			}

			# Generate body sans trailing newline
			chop $c->{body};
			$code .= $c->{body};

			# Generate output redirection
			if ($c->{output} eq 'none') {
				$code .= " >/dev/null\n";
			} elsif ($c->{output} eq 'scatter') {
				$code .= " |\n";
				my ($code2, $pipes2) = scatter_code_and_pipes($c);
				$code .= $code2;
				$pipes .= $pipes2;
			} elsif ($c->{output} eq 'file') {
				$code .= " >\$SGDIR\/npfo-$c->{file_name}.$p &\n";
				$pipes .= " \\\n\$SGDIR\/npfo-$c->{file_name}.$p";
				$parallel_file_map{$c->{file_name}} .= " \$SGDIR\/npfo-$c->{file_name}.$p";
			} elsif ($c->{output} eq 'variable') {
				error("Variables not allowed in parallel execition", $c->{line_number}) if ($p > 0);
				$code .= " >\$SGDIR\/npvo-$c->{variable_name} &\n";
				$pipes .= " \\\n\$SGDIR\/npvo-$c->{variable_name}";
			} else {
				die "Tailless command";
			}
		}
		$cmd_n++;
	}
	return ($code, $pipes);
}


# Return gather code for the string passed as an argument
sub
gather_code
{
	my($commands) = @_;
	my $code;


	$code .= "# Gather the results\n(\n";
	# Create code for all variables
	my $uname = `uname`;
	chop $uname;
	for my $gv (keys %defined_gather_variable) {
		if ($uname eq 'FreeBSD' and $opt_s eq '/bin/sh') {
			# Workaround for the FreeBSD shell
			# This doesn't execute echo "a=`sleep 5`42" &
			# in the background
			$code .= qq{\t(echo "$gv='`cat \$SGDIR/npvo-$gv`'") &\n};
		} else {
			$code .= qq{\techo "$gv='`cat \$SGDIR/npvo-$gv`'" &\n};
		}
	}

	# Create the rest of the code
	$code .= "\twait\ncat <<\\SGEOFSG\n";

	# Substitute /sgsh/... gather points with corresponding named pipe
	while ($commands =~ m|/sgsh/(\w+)|) {
		my $file_name = $1;
		error("Undefined file gather name $file_name\n") unless ($defined_gather_file{$file_name});
		$commands =~ s|/sgsh/$file_name|$parallel_file_map{$file_name}|g;
	}
	$code .= $commands;

	# -s allows passing positional arguments to subshell
	$code .= qq{SGEOFSG\n) | $opt_s -s "\$@"\n};
	return $code;
}

# Set $_ to the next line from @lines
# Return true if a next line exists
# Advance $line_number
sub
get_next_line
{
	return 0 if ($line_number > $#lines);
	$_ = $lines[$line_number++];
	return 1;
}

# Push back current line
sub
unget_line
{
	die "unget_line past beginning" if ($line_number-- == 0);
}


# Verify the code
# Ensure that all variables are used in the gather block
# Ensure that the scatter code implements a DAG
sub
verify_code
{
	my($scatter_command, $gather_commands) = @_;
}

sub
error
{
	my($message, $line) = @_;

	$line = $line_number unless(defined($line));
	print STDERR "$input_filename($line): $message\n";
	exit 1;
}
