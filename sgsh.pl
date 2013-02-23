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

# Optionally build a graph to detect cycles
my $graph;
eval 'require Graph';
if ($@) {
	print STDERR "Warning: Perl Graph module is missing.\n";
	print STDERR "Cycles in /sgsh files will not be reported.\n";
} else {
	$graph = Graph->new();
}

$main::VERSION = '0.3';

# Exit after command processing error
$Getopt::Std::STANDARD_HELP_VERSION = 1;

sub
main::HELP_MESSAGE
{
	my ($fh) = @_;
	print $fh qq{
Usage: $0 [-kng] [-s shell] [-t tee] [file]
-g		Generate a GraphViz graph of the specified processing
-k		Keep temporary script file
-n		Do not run the generated script
-o filename	Write the script in the specified file (- is stdout) and exit
-p path		Path where the sgsh helper programs are located
-s shell	Specify shell to use (/bin/sh is the default)
-t tee		Path to the teebuff command
};
}

our($opt_g, $opt_k, $opt_n, $opt_o, $opt_p, $opt_s, $opt_t);
$opt_s = '/bin/sh';
$opt_t = 'teebuff';
if (!getopts('gkno:p:s:t:')) {
	main::HELP_MESSAGE(*STDERR);
	exit 1;
}

# Ensure defined path ends with a /
$opt_p .= '/' if (defined($opt_p) && $opt_p !~ m|/$|);
$opt_p = '' unless defined($opt_p);
# Add path to opt_t unless it has one
$opt_t = "$opt_p$opt_t" unless ($opt_t =~ m|/|);

$File::Temp::KEEP_ALL = 1 if ($opt_k);

# Output file
my ($output_fh, $output_filename);

if ($opt_o) {
	# Output to specified file
	$output_filename = $opt_o;
	if ($output_filename eq '-') {
		$output_fh = \*STDOUT;
	} else {
		open($output_fh, '>', $output_filename) || die "Unable to open $output_filename: $!\n";
	}
} else {
	# Output to temporary file
	($output_fh, $output_filename) = tempfile(UNLINK => 1);
}

# Map containing all defined gather file names
my %defined_gather_file;

# Map containing all used gather file names
my %used_gather_file;

# Map containing all defined gather variable names
my %defined_gather_variable;

# Map containing all used gather variable names
my %used_gather_variable;

# Map from /sgsh/ file names into an array of named pipes
# For code and the graph structure
my %parallel_file_map;
my %parallel_graph_file_map;

# The code used for each graph node name
my %node_label;

# Ordinal number of the scatter command being processed
my $global_scatter_n;

# Lines of the input file
# Required, because we implement a double-pass algorithm
my @lines;

my $line_number = 0;

# Properties (names of lhs and rhs nodes) of graph edges
my %edge;

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

my $code = '';
my $pipes = '';
my $kvstores = '';

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

		my @gather_commands = parse_gather_command_sequence();

		# Generate graph
		print '
digraph D {
	rankdir = LR;
	node [fontname="Courier"];
' 			if ($opt_g);
		$global_scatter_n = 0;
		scatter_graph_io(\%scatter_command, 0);
		$global_scatter_n = 0;
		scatter_graph_body(\%scatter_command);
		gather_graph(\@gather_commands);
		graph_edges();

		# Now that we have the edges we can verify the graph
		verify_code(0, \%scatter_command, \@gather_commands);

		if ($opt_g) {
			print "}\n";
			exit 0;
		}

		# Generate code
		$global_scatter_n = 0;
		scatter_code_and_pipes_map(\%scatter_command);
		$global_scatter_n = 0;
		my ($code2, $pipes2, $kvstores2) = scatter_code_and_pipes_code(\%scatter_command);
		$code .= $code2;
		$pipes .= $pipes2;
		$kvstores .= $kvstores2;
		$code .= gather_code(\@gather_commands);
	} else {
		$code .= $_ ;
	}
}

# The traps ensure that the named pipe directory
# is removed on termination and that the exit code
# after a signal is that of the shell: 128 + signal number
my $stop_kvstores = $kvstores;
$stop_kvstores =~ s|\{|${opt_p}sgsh-readval -q "|g;
$stop_kvstores =~ s|\}|" 2>/dev/null\n|g;
print $output_fh q{
export SGDIR=/tmp/sg-$$
rm -rf $SGDIR
trap '} . $stop_kvstores . q{rm -rf "$SGDIR"' 0
trap 'exit $?' 1 2 3 15
mkdir $SGDIR
},
qq{
mkfifo $pipes
$code
};

# Execute the shell on the generated file
# -a inherits the shell's variables to subshell
# -n only checks for syntax errors
my @args = ($opt_s, '-n', '-a', $output_filename, @ARGV);

if ($opt_n) {
	print join(' ', @args), "\n";
	exit 0;
}

exit 0 if ($opt_o);

# Check for syntax error before executing
# This avoids leaving commands in the background
system(@args);
if ($? == -1) {
	print STDERR "Unable to execute $opt_s: $!\n";
	exit 1;
} elsif (($? >> 8) != 0) {
	$File::Temp::KEEP_ALL = 1;
	print STDERR "Syntax error in generated script $output_filename while executing $opt_s\n";
	exit 1;
}

# Now the real run
@args = ($opt_s, '-a', $output_filename, @ARGV);
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
			error("Variable \$$1 already used for output") if (defined($defined_gather_variable{$1}));
			$defined_gather_variable{$1} = 1;
			$command{output} = 'variable';
			$command{variable_name} = $1;
			$command{body} .= $_;
			return \%command;
		}

		# Gather file output endpoint: |>
		if (s/$GATHER_FILE_OUTPUT//o) {
			error("Headless command in scatter block") if (!defined($command{input}));
			error("Output file /sgsh/$1 already used for output") if (defined($defined_gather_file{$1}));
			$defined_gather_file{$1} = 1;
			undef @{$parallel_file_map{$1}};
			undef @{$parallel_graph_file_map{$1}};
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


# Parse a sequence of gather commands and return it as an array of lines
sub
parse_gather_command_sequence
{
	my @commands;

	while (get_next_line()) {
		# Gather block end: |}
		if (/$BLOCK_END/o) {
			return @commands;
		}
		push(@commands, $_);
	}
	error("End of file reached file parsing a gather block");
}

sub
parse_scatter_arguments
{
	my ($command) = @_;

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
	return ($parallel, %scatter_opts)
}

# Set the parallel file map for the specified command
sub
scatter_code_and_pipes_map
{
	my($command) = @_;

	my $scatter_n = $global_scatter_n++;

	my ($parallel, %scatter_opts) = parse_scatter_arguments($command);

	# Process the commands
	my $cmd_n = 0;
	for my $c (@{$command->{scatter_commands}}) {
		for (my $p = 0; $p < $parallel; $p++) {

			# Pass-through fast exit
			if ($c->{input} eq 'scatter' && $c->{body} eq '' && $c->{output} eq 'file') {
				push(@{$parallel_file_map{$c->{file_name}}}, "\$SGDIR\/npfo-$c->{file_name}.$p");
				next;
			}

			# Generate output redirection
			if ($c->{output} eq 'scatter') {
				my ($code2, $pipes2) = scatter_code_and_pipes_map($c);
			} elsif ($c->{output} eq 'file') {
				push(@{$parallel_file_map{$c->{file_name}}}, "\$SGDIR\/npfo-$c->{file_name}.$p");
			}
		}
		$cmd_n++;
	}
}


# Return the code and the named pipes that must be generated
# to scatter data for the specified command
sub
scatter_code_and_pipes_code
{
	my($command) = @_;
	my $code = '';
	my $pipes = '';
	my $kvstores = '';

	my $scatter_n = $global_scatter_n++;

	my ($parallel, %scatter_opts) = parse_scatter_arguments($command);

	# Count number of scatter targets;
	my $commands = $command->{scatter_commands};

	my $scatter_targets = 0;
	for my $c (@{$commands}) {
		$scatter_targets++ if ($c->{input} eq 'scatter');
	}

	# Create tee, if needed
	if ($scatter_targets * $parallel > 1) {
		# Create arguments
		my $tee_args = '';
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
			my $body = $c->{body};
			chop $body;
			# Substitute /sgsh/... gather points with corresponding named pipe
			while ($body =~ m|/sgsh/(\w+)|) {
				my $file_name = $1;
				$body =~ s|/sgsh/$file_name|join(' ', @{$parallel_file_map{$file_name}})|eg;
			}
			$code .= $body;

			# Generate output redirection
			if ($c->{output} eq 'none') {
				$code .= " >/dev/null\n";
			} elsif ($c->{output} eq 'scatter') {
				$code .= " |\n";
				my ($code2, $pipes2, $kvstores2) = scatter_code_and_pipes_code($c);
				$code .= $code2;
				$pipes .= $pipes2;
				$kvstores .= $kvstores2;
			} elsif ($c->{output} eq 'file') {
				$code .= " >\$SGDIR/npfo-$c->{file_name}.$p &\n";
				$pipes .= " \\\n\$SGDIR/npfo-$c->{file_name}.$p";
			} elsif ($c->{output} eq 'variable') {
				error("Variables not allowed in parallel execution", $c->{line_number}) if ($p > 0);
				$code .= " | ${opt_p}sgsh-writeval \$SGDIR/$c->{variable_name} &\n";
				$kvstores .= "{\$SGDIR/$c->{variable_name}}";
			} else {
				die "Tailless command";
			}
		}
		$cmd_n++;
	}
	return ($code, $pipes, $kvstores);
}


# Return gather code for the string passed as an argument
sub
gather_code
{
	my($commands) = @_;
	my $code;

	$code .= "# Gather the results\n";
	# Set the variables
	for my $var (keys %defined_gather_variable) {
		# Ask for last record (-l) and quit the server (-q)
		# To avoid race condition with the socket setup code
		# ask to retry connection to socket if it is missing (-r)
		$code .= qq[$var="\`${opt_p}sgsh-readval -lqr \$SGDIR\/$var\`"\n];
	}

	for my $command (@{$commands}) {
		# Substitute /sgsh/... gather points with corresponding named pipe
		while ($command =~ m|/sgsh/(\w+)|) {
			my $file_name = $1;
			$command =~ s|/sgsh/$file_name|join(' ', @{$parallel_file_map{$file_name}})|eg;
		}
		$code .= $command;
	}

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
# Ensure that all /sgsh files are used exactly once
# Ensure that all variables are used in the gather block
# Ensure that the scatter code implements a DAG
sub
verify_code
{
	my($level, $scatter_command, $gather_commands) = @_;

	my $commands = $scatter_command->{scatter_commands};
	for my $c (@{$commands}) {
		my $body = $c->{body};
		chop $body;
		while ($body =~ s|/sgsh/(\w+)||) {
			my $file_name = $1;
			error("Undefined gather file name /sgsh/$file_name specified for input\n") unless ($defined_gather_file{$file_name});
			error("Gather file name /sgsh/$file_name used for input more than once\n") if ($used_gather_file{$file_name});
			$used_gather_file{$file_name} = 1;
		}

		if ($c->{output} eq 'scatter') {
			verify_code($level + 1, $c);
		}
	}

	for my $command (@{$gather_commands}) {
		my $tmp_command = $command;
		while ($tmp_command =~ s|/sgsh/(\w+)||) {
			my $file_name = $1;
			error("Undefined gather file name /sgsh/$file_name specified for input\n") unless ($defined_gather_file{$file_name});
			error("Gather file name /sgsh/$file_name used for input more than once\n") if ($used_gather_file{$file_name});
			$used_gather_file{$file_name} = 1;
		}
		while ($tmp_command =~ s|\$(\w+)||) {
			$used_gather_variable{$1} = 1;
		}
	}

	if ($level == 0) {
		for my $gf (keys %defined_gather_file) {
			error("Gather file /sgsh/$gf is never read\n") unless (defined($used_gather_file{$gf}));
		}
		for my $gv (keys %defined_gather_variable) {
			warning("Gather variable \$$gv set but not used\n") unless (defined($used_gather_variable{$gv}));
		}

		my @cycle;
		if (defined($graph) && (@cycle = $graph->find_a_cycle())) {
			print STDERR "The following dependencies across /sgsh files form a cycle:\n";
			for my $node (@cycle) {
				print "$node_label{$node}\n";
			}
			exit 1;
		}
	}
}

# Report an error and exit
sub
error
{
	my($message, $line) = @_;

	$line = $line_number unless(defined($line));
	print STDERR "$input_filename($line): $message\n";
	exit 1;
}

# Report a warning (don't exit)
sub
warning
{
	my($message, $line) = @_;

	$line = $line_number unless(defined($line));
	print STDERR "$input_filename($line): $message\n";
}

# Process the I/O redirection specifications returning
# the name of the tee node and a list of edges to which the specified command scatters the data
# setting the %edge and %parallel_graph_file_map
sub
scatter_graph_io
{
	my($command, $level) = @_;
	my @output_edges;

	my $scatter_n = $global_scatter_n++;

	my $show_teebuff = 0;

	my ($parallel, %scatter_opts) = parse_scatter_arguments($command);

	# Count number of scatter targets;
	my $commands = $command->{scatter_commands};

	my $scatter_targets = 0;
	for my $c (@{$commands}) {
		$scatter_targets++ if ($c->{input} eq 'scatter');
	}

	my $tee_node_name = "node_tee_$scatter_n";
	# Create tee, if needed
	if ($scatter_targets * $parallel > 1) {
		$show_teebuff = 1 if ($level == 0);
		# Create arguments
		my $tee_args = '';
		if ($scatter_opts{'s'}) {
			$tee_args .= ' -s';
			$show_teebuff = 1;
		}
		if ($scatter_opts{'l'}) {
			$tee_args .= ' -l';
			$show_teebuff = 1;
		}
		for (my $cmd_n = 0; $cmd_n  < $scatter_targets; $cmd_n++) {
			for (my $p = 0; $p < $parallel; $p++) {
				$edge{"npi-$scatter_n.$cmd_n.$p"}->{lhs} = $tee_node_name;
			}
		}
		# Obtain tee program
		my $tee_prog = $opt_t;
		$tee_prog = $scatter_opts{'t'} if ($scatter_opts{'t'});

		print qq{\t$tee_node_name [label="$tee_prog $tee_args"];\n} if ($opt_g && $show_teebuff);
	}

	# Process the commands
	# Pass 1: I/O redirection
	my $cmd_n = 0;
	for my $c (@{$commands}) {
		for (my $p = 0; $p < $parallel; $p++) {

			# Pass-through fast exit
			if ($c->{input} eq 'scatter' && $c->{body} eq '' && $c->{output} eq 'file') {
				$edge{"npfo-$c->{file_name}.$p"}->{lhs} = $edge{"npi-$scatter_n.$cmd_n.$p"}->{lhs};
				$edge{"npi-$scatter_n.$cmd_n.$p"}->{passthru} = 1;
				push(@{$parallel_graph_file_map{$c->{file_name}}}, "npfo-$c->{file_name}.$p");
				next;
			}

			my $node_name = "node_cmd_${scatter_n}_${cmd_n}_$p";

			# Generate input
			if ($c->{input} eq 'none') {
				;
			} elsif ($c->{input} eq 'scatter') {
				push(@output_edges, "npi-$scatter_n.$cmd_n.$p");
				$edge{"npi-$scatter_n.$cmd_n.$p"}->{rhs} = $node_name;
			} else {
				die "Headless command";
			}

			# Generate output redirection
			if ($c->{output} eq 'none') {
				;
			} elsif ($c->{output} eq 'scatter') {
				my ($tee_node_name, @output_edges) = scatter_graph_io($c, $level + 1);
				# Generate scatter edges
				for my $output_edge (@output_edges) {
					$edge{$output_edge}->{lhs} =
						 ($#output_edges > 0 && $show_teebuff) ? $tee_node_name : $node_name;
				}
				# Connect our command to teebuff
				print qq{\t$node_name -> $tee_node_name\n} if ($opt_g && $#output_edges > 0 && $show_teebuff);
			} elsif ($c->{output} eq 'file') {
				$edge{"npfo-$c->{file_name}.$p"}->{lhs} = $node_name;
				push(@{$parallel_graph_file_map{$c->{file_name}}}, "npfo-$c->{file_name}.$p");
			} elsif ($c->{output} eq 'variable') {
				error("Variables not allowed in parallel execution", $c->{line_number}) if ($p > 0);
				print qq(\t$node_name -> "\$$c->{variable_name}";\n) if ($opt_g);
			} else {
				die "Tailless command";
			}
		}
		$cmd_n++;
	}

	return ($tee_node_name, @output_edges);
}


# Pass 2: Having established parallel_graph_file_map process the bodies
sub
scatter_graph_body
{
	my($command) = @_;
	my $scatter_n = $global_scatter_n++;

	my ($parallel, %scatter_opts) = parse_scatter_arguments($command);

	my $cmd_n = 0;
	for my $c (@{$command->{scatter_commands}}) {
		for (my $p = 0; $p < $parallel; $p++) {
			next if ($c->{input} eq 'scatter' && $c->{body} eq '' && $c->{output} eq 'file');

			my $node_name = "node_cmd_${scatter_n}_${cmd_n}_$p";

			# Generate body sans trailing newline and update corresponding edges
			my $body = $c->{body};
			chop $body;
			while ($body =~ s|/sgsh/(\w+)||) {
				my $file_name = $1;
				for my $file_name_p (@{$parallel_graph_file_map{$file_name}}) {
					$edge{"$file_name_p"}->{rhs} = $node_name;
				}
			}
			print qq{\t$node_name [label="} . graphviz_escape($body) . qq{"];\n} if ($opt_g);
			$node_label{$node_name} = $body;

			if ($c->{output} eq 'scatter') {
				scatter_graph_body($c);
			}
		}
		$cmd_n++;
	}
}


# Escape characters making the argument a valid GraphViz string
sub
graphviz_escape
{
	my ($name) = @_;

	$name =~ s/\\/\\\\/g;
	$name =~ s/"/\\"/g;
	$name =~ s/\n\s*/\\l/g;
	$name =~ s/\s+$//;
	$name =~ s/^\s+//;
	$name =~ s/$/\\l/;
	return $name;
}

# Create nodes and edges for the gather commands passed as an array reference
sub
gather_graph
{
	my($commands) = @_;
	my $n = 0;

	for my $command (@{$commands}) {
		my $is_node = 0;
		my $node_name = "gather_node_$n";
		my $command_tmp = $command;
		while ($command_tmp =~ s|/sgsh/(\w+)||) {
			my $file_name = $1;
			for my $file_name_p (@{$parallel_graph_file_map{$file_name}}) {
				$edge{"$file_name_p"}->{rhs} = $node_name;
			}
			$is_node = 1;
		}
		print qq{\t$node_name [label="} . graphviz_escape($command_tmp) . qq{"];\n} if ($opt_g && $is_node);
		$n++;
	}
}

# Draw the graph's edges
sub
graph_edges
{
	my $u = 0;

	for my $e (keys %edge) {
		next if ($edge{$e}->{passthru});
		if (!defined($edge{$e}->{lhs})) {
			print qq{\tunknown_node_$u [label="???"]; // $e\n} if ($opt_g);
			$edge{$e}->{lhs} = "unknown_node_$u";
			$u++;
		}
		if (!defined($edge{$e}->{rhs})) {
			print qq{\tunknown_node_$u [label="???"]; // $e\n} if ($opt_g);
			$edge{$e}->{rhs} = "unknown_node_$u";
			$u++;
		}
		$graph->add_edge($edge{$e}->{lhs}, $edge{$e}->{rhs}) if (defined($graph));
		print "\t$edge{$e}->{lhs} -> $edge{$e}->{rhs}; // $e\n" if ($opt_g);
	}
}
