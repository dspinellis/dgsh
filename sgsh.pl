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

use Cwd 'abs_path';
use File::Temp 'tempfile';
use File::Copy;
use Getopt::Std;
use IO::Handle;

# Optionally build a graph to detect cycles
my $graph;
eval 'require Graph';
if ($@) {
	print STDERR "Warning: Perl Graph module is missing.\n";
	print STDERR "Cycles in /stream files will not be reported.\n";
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
Usage: $0 [-g style] [-dkmnS] [-o file] [-p path] [-s shell] [-t tee] [file]
-d		Generate and launch debug and monitoring interface
-g style	Generate a GraphViz graph of the specified processing:
		"plain" gives full details in B&W Courier
		"pretty" reduces details, adds colors, Arial font
		"pretty-full" adds colors, Arial font
-k		Keep temporary script file
-m		Instrument code with monitoring probes
-n		Do not run the generated script
-o filename	Write the script in the specified file (- is stdout) and exit
-p path		Path where the sgsh helper programs are located
-s shell	Specify shell to use (/bin/sh is the default)
-S		Set barriers at the scatter points and use temporary files
-t tee		Path to the sgsh-tee command
};
}

our($opt_d, $opt_g, $opt_k, $opt_m, $opt_n, $opt_o, $opt_p, $opt_s, $opt_S, $opt_t);
$opt_s = '/bin/sh';
$opt_t = 'sgsh-tee';
if (!getopts('dg:kmno:p:s:St:')) {
	main::HELP_MESSAGE(*STDERR);
	exit 1;
}

if ($opt_S && ($opt_d || $opt_m)) {
	print STDERR "The -S option is not compatible with -d or -m\n";
	exit 1;
}

# Make path absolute
$opt_p = abs_path($opt_p) if (defined($opt_p));

# Ensure defined path ends with a /
$opt_p .= '/' if (defined($opt_p) && $opt_p !~ m|/$|);
$opt_p = '' unless defined($opt_p);
# Add path to opt_t unless it has one
$opt_t = "$opt_p$opt_t" unless ($opt_t =~ m|/|);

$File::Temp::KEEP_ALL = 1 if ($opt_k);
my $temp_name = '/tmp/sg-XXXXXXX';

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
	($output_fh, $output_filename) = tempfile($temp_name, UNLINK => 1);
}

# GraphViz attributes: all nodes, stores, processing nodes
my $gv_node_attr;
my $gv_edge_attr = '[]';
my $gv_store_attr;
my $gv_proc_attr;

# GraphViz graph output file
my $graph_out;
# One of pretty, pretty-full, plain
my $graph_style;

if ($opt_g) {
	$graph_out = \*STDOUT;
	$graph_style = $opt_g;
}

my $graph_filename;
if ($opt_d) {
	# Verify that dot is available; Later, with redirection, we get an
	# error from the shell execution.
	system('dot', '/dev/null');
	if ($? == -1) {
		print STDERR "Unable to execute dot: $!\n";
		exit 1;
	}
	($graph_out, $graph_filename) = tempfile($temp_name, UNLINK => 1);
	$graph_style = 'pretty';
	$gv_edge_attr = ' [penwidth="3pt", color="grey"]';
	# Create monitoring ports
	$opt_m = 1;
}

if ($graph_out) {
	if ($graph_style eq 'pretty' || $graph_style eq 'pretty-full') {
		$gv_node_attr = '[fontname="Arial", gradientangle="90", style="filled"]';
		$gv_store_attr = 'shape="box", fillcolor="cyan:white"';
		$gv_proc_attr = 'shape="ellipse", fillcolor="yellow:white"';
	} elsif ($graph_style eq 'plain') {
		$gv_node_attr = '[fontname="Courier"]';
		$gv_store_attr = 'shape="box"';
		$gv_proc_attr = 'shape="ellipse"';
	} else {
		main::HELP_MESSAGE(*STDERR);
		exit 1;
	}
}


# Map containing all defined gather file names
my %defined_stream;

# Map containing all used gather file names
my %used_stream;

# Map containing all defined gather store names
my %defined_store;

# Map containing all used gather store names
my %used_store;

# Map containing all created named pipes for scatter commands
my %written_pipe;

# Map containing all symbolically linked named pipes for scatter commands
my %linked_pipe;

# Map containing all used named pipes for scatter commands
my %read_pipe;

# Map from /stream/ file names into an array of named pipes
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

# Outputs for HTML file used for debugging
my ($html_out, $html_filename);

# Location where sgsh files reside (call set_library_path to set)
my $library_path;

# Ordinal of the scatter-gather block being processed
my $block_ordinal = 0;

# File names of named pipes that must be waited on at program end
# (by reading until EOF), in order to allow the corresponding processes
# whose blocks are delimited with |. to run to completion.
my @gather_collect;

# Regular expressions for the input files scatter-gather operators
# scatter |{
my $SCATTER_BLOCK_BEGIN = q!^[^'#"]*scatter\s*\|\{\s*(.*)?$!;
# |{
my $SCATTER_BEGIN = q!\|\{\s*(.*)?$!;
# |} gather |{ or |}
my $SCATTER_BLOCK_END = q!^[^'#"]*\|\}((\s*gather\s*\|\{\s*\#.*)|(.*)$)?!;
# |} gather |{
my $GATHER_BLOCK_BEGIN = q!^[^'#"]*\|\}\s*gather\s*\|\{\s*(\#.*)?$!;
# |} [redirection]
my $GATHER_BLOCK_END = q!^[^'#"]*\|\}(.*)$!;
# |}
my $BLOCK_END = q!^[^'#"]*\|\}\s*(\#.*)?$!;
# -|
my $SCATTER_INPUT = q!^[^'#"]*-\|!;
# .|
my $NO_INPUT = q!^[^'#"]*\.\|!;
# |store:name
my $GATHER_STORE_OUTPUT = q!\|\s*store\:(\w+)\s*([^#]*)(\#.*)?$!;
# |>/stream/name
my $GATHER_STREAM_OUTPUT = q!\|\>\s*\/stream\/([\w.\-+=,]+)\s*(\#.*)?$!;
# |.
my $NO_OUTPUT = q!\|\.\s*(\#.*)?$!;
# -||>/stream/name
my $SCATTER_GATHER_PASS_THROUGH = q!^[^'#"]*-\|\|\>\s*\/stream\/([\w.\-+=,]+)\s*(\#.*)?$!;
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

# Remove spaces passwd by the Linux kernel in #!
$opt_s =~ s/^\s+//;

# Generate code heading
print $output_fh "#!$opt_s
# Automatically generated file
# Source file $input_filename
";

my $code = '';

# Parse the file and generate the corresponding code
while (get_next_line()) {
	# Scatter block begin: scatter |{
	if (!/$COMMENT_LINE/o && /$SCATTER_BLOCK_BEGIN/o) {

		clear_global_variables();

		# Top-level scatter command
		my %scatter_command;
		$scatter_command{line_number} = $line_number;
		# The fd3 redirections allow piping into the scatter block
		# POSIX mandates that background commands (like the tee pipeline starting the scatter block)
		# have their standard input redirected to /dev/null.
		# Our use of fd3 allows the tee command to regain access to stdin
		$scatter_command{input} = 'fd3';
		$scatter_command{output} = 'scatter';
		$scatter_command{body} = '';
		$scatter_command{scatter_flags} = $1;
		$scatter_command{scatter_commands} = parse_scatter_command_sequence($SCATTER_BLOCK_END);

		my ($redirection, @gather_commands);
		m/$SCATTER_BLOCK_END/;	# Match again to check redirection
		$redirection = $3;	# Redirection without a gather block
		# Redirection and gather commands in a gather block
		($redirection, @gather_commands) = parse_gather_command_sequence() if (/$GATHER_BLOCK_BEGIN/);

		$global_scatter_n = 0;
		scatter_graph_io(\%scatter_command, 0);
		$global_scatter_n = 0;
		scatter_graph_body(\%scatter_command);
		gather_graph(\@gather_commands);
		graph_edges();

		# Now that we have the edges we can verify the graph
		verify_code(0, \%scatter_command, \@gather_commands);

		print $graph_out "}\n" if ($graph_out);
		next if ($opt_g);

		# Generate code
		$global_scatter_n = 0;
		scatter_code_and_pipes_map(\%scatter_command);
		$global_scatter_n = 0;
		my ($code2, $kvstores) = scatter_code_and_pipes_code(\%scatter_command, '');

		# Wait for processes that are running in blocks delimited by |.
		# No other process depends on these processes (because they don't scatter
		# output or send it to a stream). Therefore, they would normally
		# be killed after any other gather programs terminate (or immediately
		# if no gather code is given).
		# Waiting is done by reading the associated pipes until EOF, in order to allow
		# the corresponding processes to run to completion.
		# Use sgsh-tee, rather than cat, to open all named pipes immediately.
		my $no_output_wait = '';

		if ($#gather_collect != -1) {
			$no_output_wait = "${opt_p}sgsh-tee " .
				join(' ', map(" -i \$SGDIR/$_", @gather_collect)) .
				" >/dev/null\n";
			grep($read_pipe{$_} = 1, @gather_collect);
		}

		# Prompt to exit debugging if debugging from an interactive session
		my $debug_wait = $opt_d ? qq{
		if [ -t 0 ]
		then
			echo -n 'Press enter to terminate the debugging session'
			read sgsh_return
		fi
		} : '' ;

		# Handle redirection
		# 1. Remove line comments
		$redirection =~ s/\#.*//;
		# 2. Pipe redirection should follow 3<&0, input redirection should precede it
		my $redirection2 = '';
		if ($redirection =~ m/(.*?)(\|.*)/) {
			$redirection = $1;
			$redirection2 = $2;
		}
		# 3. The fd3 redirections allow piping into the scatter block
		$redirection = "$redirection 3<&0 $redirection2";

		$code .= "(\n" .
			initialization_code($kvstores) .
			"$code2\n" .
			debug_code() .
			gather_code(\@gather_commands) .
			$no_output_wait .
			$debug_wait .
			"\n) $redirection\n";		
		debug_create_html();
	} else {
		$code .= $_ ;
	}
}

# Graph
exit 0 if ($opt_g);

print $output_fh $code;

# Execute the shell on the generated file
# -a inherits the shell's variables to subshell
# -n only checks for syntax errors
my @args = ($opt_s, '-n', '-a', $output_filename, @ARGV);

print join(' ', @args), "\n" if ($opt_n);

# Verify the consistency of the generated code
verify_generated_code();

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

exit 0 if ($opt_n);

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

# Clear the global variables used for processing each scatter-gather blog
sub
clear_global_variables
{
	undef %defined_stream;
	undef %used_stream;
	undef %defined_store;
	undef %used_store;
	undef %written_pipe;
	undef %linked_pipe;
	undef %read_pipe;
	undef %parallel_file_map;
	undef %parallel_graph_file_map;
	undef %node_label; # ?
	undef @gather_collect;
	$graph = Graph->new() if (defined($graph));
}

# Return the code to initialize a scatter-gather block
sub
initialization_code
{
	my ($kvstores) = @_;

	my $stop_httpd = '';
	if ($opt_d) {
		$kvstores .= "{\$SGDIR/.SG_HTTP_PORT}";
		$stop_httpd = "curl -s40 http://localhost:\$SG_HTTP_PORT/.server?quit 2>&1 >/dev/null\n";
	}

	my $stop_kvstores;
	if ($opt_S) {
		$stop_kvstores = '';
	} else {
		$stop_kvstores = $kvstores;
		$stop_kvstores =~ s|\{|${opt_p}sgsh-readval -q -s "|g;
		$stop_kvstores =~ s|\}|" 2>/dev/null\n|g;
	}

	# The traps ensure that the named pipe directory
	# is removed on termination and that the exit code
	# after a signal is that of the shell: 128 + signal number
	return q{
	export SGDIR=/tmp/sg-$$.} . $block_ordinal++ . q@

	rm -rf $SGDIR

	# Cleanup on exit or interrupt
	cleanup()
	{
		SIGNAL=$1
		[ $SIGNAL = EXIT ] || echo sgsh interrupted. Cleaning up... 1>&2

		# Stop key-value stores
		@ . $stop_kvstores . $stop_httpd . q(
		# Kill processes we have launched in the background
		kill $SGPID 2>/dev/null

		# Remove temporary directory
		rm -rf "$SGDIR"

		# Propagate real signals and exit with non-0
		if [ $SIGNAL != EXIT ]
		then
			trap - $SIGNAL EXIT
			kill -s $SIGNAL $$
		fi

		# Exit with the original exit value
		exit

	}

	for sig in HUP INT QUIT TERM EXIT
	do
		trap "cleanup $sig" $sig
	done

	mkdir $SGDIR
	) .
	($opt_S ? '' :
		(
			"\nmkfifo " .
			join(" \\\n", map("\t\$SGDIR/$_", sort keys %written_pipe)) .
			"\n\n"
		)
	) .
	debug_initialize();
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
#	 '|store:' varname |
#	 '|{' flags scatter_command_sequence |
#	 '|.')
# The return is a hash with the following elements:
# input: 		none|scatter|fd[0-9]+
# body:			Command's text
# output:		none|scatter|stream|store
# scatter_commands:	When output = scatter
# scatter_flags:	When output = scatter
# store_name:		When output = store
# file_name:		When output = stream
# store_flags:		When output = store
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

		# Gather store output endpoint: |store:
		if (s/$GATHER_STORE_OUTPUT//o) {
			error("Headless command in scatter block") if (!defined($command{input}));
			error("Store store:$1 already used for output") if (defined($defined_store{$1}));
			$defined_store{$1} = 1;
			$command{output} = 'store';
			$command{store_name} = $1;
			$command{store_flags} = $2;
			$command{store_flags} =~ s/\n//;
			$command{body} .= $_;
			return \%command;
		}

		# Gather file output endpoint: |>
		if (s/$GATHER_STREAM_OUTPUT//o) {
			error("Headless command in scatter block") if (!defined($command{input}));
			error("Output stream /stream/$1 already used for output") if (defined($defined_stream{$1}));
			$defined_stream{$1} = 1;
			undef @{$parallel_file_map{$1}};
			undef @{$parallel_graph_file_map{$1}};
			$command{output} = 'stream';
			$command{file_name} = $1;
			$command{body} .= $_;
			return \%command;
		}

		# Gather no output endpoint: |.
		if (s/$NO_OUTPUT//o) {
			error("Headless command in scatter block") if (!defined($command{input}));
			$command{output} = 'none';
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
	my $state = 'plain';
	my $here_delim;		# HERE document delimiter

	my $command;
	while (get_next_line()) {
		# Gather block end: |}
		if (/$GATHER_BLOCK_END/o) {
			# $1 is the optional block redirection
			return ($1, @commands);
		}

		# Attempt to handle multi-line commands
		if ($state eq 'plain' || $state eq 'continuation') {
			if ($state eq 'plain') {
				$command->{line_number} = $line_number;
				$command->{body} = $_;
			} else {
				$command->{body} .= $_;
			}
			if (/<<-?(.*)/) {
				$here_delim = $1;
				$here_delim =~ s/[\'\"\\]//g;
				$state = 'here';
			} elsif (/^\s*(\#.*)?$/ && $state eq 'continuation') {
				;
			} elsif (/\\$/ || /\|\s*(\#.*)?$/) {
				$state = 'continuation';
			} else {
				push(@commands, $command);
				undef $command;
				$state = 'plain';
			}
		} elsif ($state eq 'here') {
			$command->{body} .= $_;
			if (/^$here_delim$/) {
				$state = 'plain';
				push(@commands, $command);
				undef $command;
			}
		} else {
			internal_error("parse_gather_command_sequence: Illegal state $state");
		}
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
	# d Direct (no buffering in parallel)
	# l Passed to tee
	# p Parallel invocations
	# s Passed to tee
	# t Specify the tee program
	getopts('dlp:st:', \%scatter_opts);
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

# Set the parallel file map @parallel_file_map for the specified command
# This is a map from /stream/ file names into an array of named pipes
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
			if ($c->{input} eq 'scatter' && $c->{body} eq '' && $c->{output} eq 'stream') {
				push(@{$parallel_file_map{$c->{file_name}}}, "npfo-$c->{file_name}.$p");
				next;
			}

			# Generate output redirection
			if ($c->{output} eq 'scatter') {
				scatter_code_and_pipes_map($c);
			} elsif ($c->{output} eq 'stream') {
				push(@{$parallel_file_map{$c->{file_name}}}, "npfo-$c->{file_name}.$p");
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
	my($command, $monitor_scatter_pid) = @_;
	my $code = '';
	my $kvstores = '';
	# Open the bracket unless opened by a preceding command (recursive call)
	my $monitor_open_bracket = ($opt_m && !$monitor_scatter_pid) ? ' ( ' : '';
	my $monitor_close_bracket = $opt_m ? ' ) ' : '';

	my $scatter_n = $global_scatter_n++;

	my ($parallel, %scatter_opts) = parse_scatter_arguments($command);

	# Count number of scatter targets;
	my $commands = $command->{scatter_commands};

	my $scatter_targets = 0;
	for my $c (@{$commands}) {
		$scatter_targets++ if ($c->{input} eq 'scatter');
	}

	warning("Scatter to a single target") if ($scatter_targets * $parallel == 1);

	# Create tee, if needed
	if ($scatter_targets * $parallel >= 1) {
		# Create arguments
		my $tee_args = '';
		$tee_args .= ' -s' if ($scatter_opts{'s'});
		$tee_args .= ' -l' if ($scatter_opts{'l'});

		my $cmd_n = 0;
		for my $c (@{$commands}) {
			if ($c->{input} eq 'scatter') {
				for (my $p = 0; $p < $parallel; $p++) {
					$tee_args .= " -o \$SGDIR/npi-$scatter_n.$cmd_n.$p";
					$written_pipe{"npi-$scatter_n.$cmd_n.$p"} = 1;
				}
			}
			$cmd_n++;
		}

		# Obtain tee program
		my $tee_prog = $opt_t;
		$tee_prog = $scatter_opts{'t'} if ($scatter_opts{'t'});


		if ($opt_S) {
			# Sequential code: send output to first file, and link the others to it
			error("-S not compatible with -s", $command->{line_number}) if ($scatter_opts{'s'});
			# The fdn redirection allows piping into the scatter block
			$code .= "cat <&$1 $1<&- " if ($command->{input} =~ m/fd([0-9]+)/);
			my $pipes = $tee_args;
			$pipes =~ s/\s+-\w//g;
			my @pipes2 = split(/[\s\\]+/, $pipes);
			# To keep the code simple, we link the one output file with the rest
			$code .= " >$pipes2[1]\n";
			for my $name (@pipes2[2 .. $#pipes2]) {
				$code .= "ln $pipes2[1] $name\n";
			}
		} else {
			# Scatter code: asynchronous tee to named pipes
			# The fdn redirection allows piping into the scatter block
			$tee_args .= " <&$1 $1<&- " if ($command->{input} =~ m/fd([0-9]+)/);
			$code .= qq[$monitor_open_bracket$tee_prog $tee_args $monitor_close_bracket & SGPID="\$! \$SGPID"$monitor_scatter_pid\n];
		}
	}

	# Process the commands
	my $cmd_n = 0;
	for my $c (@{$commands}) {
		for (my $p = 0; $p < $parallel; $p++) {

			# Pass-through fast exit
			if ($c->{input} eq 'scatter' && $c->{body} eq '' && $c->{output} eq 'stream' && !$opt_m) {
				$code .= "ln -s \$SGDIR\/npi-$scatter_n.$cmd_n.$p \$SGDIR\/npfo-$c->{file_name}.$p\n";
				$read_pipe{"npi-$scatter_n.$cmd_n.$p"} = 1;
				$linked_pipe{"npfo-$c->{file_name}.$p"} = 1;
				next;
			}

			# In sequential code set variables rather than output to stores
			$code .= "$c->{store_name}=\$( " if ($opt_S && $c->{output} eq 'store');

			# Body sans trailing newline
			my $body = $c->{body};
			chop $body;

			my $monitor = '';
			if ($opt_m && $c->{input} eq 'scatter') {
				# Split the data into a monitoring and a use path
				$monitor = '.use';
				# Use tee(1) here to achieve tight monitoring
				$code .= qq{tee \$SGDIR/npi-$scatter_n.$cmd_n.$p.use >\$SGDIR/npi-$scatter_n.$cmd_n.$p.monitor <\$SGDIR/npi-$scatter_n.$cmd_n.$p & SGPID="\$! \$SGPID"\n};
				$read_pipe{"npi-$scatter_n.$cmd_n.$p"} = 1;
				$written_pipe{"npi-$scatter_n.$cmd_n.$p.use"} = 1;
				$written_pipe{"npi-$scatter_n.$cmd_n.$p.monitor"} = 1;
				$code .= qq[${opt_p}sgsh-monitor <\$SGDIR/npi-$scatter_n.$cmd_n.$p.monitor | ${opt_p}sgsh-writeval -s \$SGDIR/mon-npi-$scatter_n.$cmd_n.$p & SGPID="\$! \$SGPID"\n];
				$read_pipe{"npi-$scatter_n.$cmd_n.$p.monitor"} = 1;
				$kvstores .= "{\$SGDIR/mon-npi-$scatter_n.$cmd_n.$p}";
				$body = 'cat' if ($body eq '');
			}

			my $monitor_pid = '';
			my $monitor_close_bracket = '';
			if ($opt_m) {
				# Code to store a command's pid into a file when monitoring
				$monitor_pid .= qq[ ; echo '{ "pid" : "'\$!'" }' >\$SGDIR/pid-node_cmd_${scatter_n}_${cmd_n}_$p.json];
				$monitor_close_bracket = ')';
				# Opening bracket to obtain subshell pid and brace to redirect I/O as if the commands were one
				$code .= ' ( { '; # }
			} else {
				# Opening brace to redirect I/O as if the commands were one
				$code .= ' { '; # }
			}

			$code .= substitute_streams_and_stores($body);

			# Buffer output if parallel scatter
			# Parallel execution probably implies high-latency
			# upstream commands.  Buffering their output ensures
			# that the downstream merge won't block waiting for
			# one of them and thus also block the other upstream
			# ones.
			$code .= " | ${opt_p}sgsh-tee -I"
				if ($parallel > 1 && !$scatter_opts{'d'});

			# Closing brace to redirect I/O as if the commands were one
			# If no output append a null command to ensure npfo-none will
			# stay open if the last command redirects its output
			if ($c->{output} eq 'none') {
				$code .= "\n: ; }";
			} else {
				$code .= "\n}";
			}

			# Generate input
			if ($c->{input} eq 'none') {
				$code .= '</dev/null ';
			} elsif ($c->{input} eq 'scatter') {
				$code .= " <\$SGDIR/npi-$scatter_n.$cmd_n.$p$monitor";
				$read_pipe{"npi-$scatter_n.$cmd_n.$p$monitor"} = 1;
			} else {
				internal_error("Headless command");
			}

			# Generate output redirection
			if ($c->{output} eq 'none') {
				my $out_name = "npfo-none-$scatter_n.$cmd_n.$p";
				if ($opt_S) {
					# Sequential code execution
					$code .= qq[ >\$SGDIR/$out_name\n];
				} else {
					$code .= qq[ >\$SGDIR/$out_name $monitor_close_bracket & SGPID="\$! \$SGPID"$monitor_pid\n];
				}
				$written_pipe{$out_name} = 1;
				# Save file name to be waited on later
				push(@gather_collect, $out_name);
			} elsif ($c->{output} eq 'scatter') {
				my ($code2, $kvstores2) = scatter_code_and_pipes_code($c, $monitor_pid);
				if ($c->{body} ne '' && !$opt_S) {
					$code .= " |\n";
				}
				$code .= $code2;
				$kvstores .= $kvstores2;
			} elsif ($c->{output} eq 'stream') {
				if ($opt_S) {
					# Sequential code execution
					$code .= qq{ >\$SGDIR/npfo-$c->{file_name}.$p\n};
				} else {
					if ($opt_m) {
						# Pipe to monitor process and close subshell bracket
						$code .= qq[ | tee \$SGDIR/npfo-$c->{file_name}.$p >\$SGDIR/npfo-$c->{file_name}.$p.monitor ) & SGPID="\$! \$SGPID"$monitor_pid\n];
						$written_pipe{"npfo-$c->{file_name}.$p.monitor"} = 1;
						$code .= qq[${opt_p}sgsh-monitor <\$SGDIR/npfo-$c->{file_name}.$p.monitor | ${opt_p}sgsh-writeval -s \$SGDIR/mon-npfo-$c->{file_name}.$p & SGPID="\$! \$SGPID"\n];
						$read_pipe{"npfo-$c->{file_name}.$p.monitor"} = 1;
						$kvstores .= "{\$SGDIR/mon-npfo-$c->{file_name}.$p}";
					} else {
						$code .= qq[ >\$SGDIR/npfo-$c->{file_name}.$p & SGPID="\$! \$SGPID"\n];
					}
				}
				$written_pipe{"npfo-$c->{file_name}.$p"} = 1;
			} elsif ($c->{output} eq 'store') {
				error("Stores not allowed in parallel execution", $c->{line_number}) if ($p > 0);
				if ($opt_S) {
					# Close command redirection
					$code .= " )\n";
				} else {
					$code .= ' |' unless $c->{body} eq '';
					if ($opt_m) {
						# Pipe to monitor process and close subshell bracket
						# Use tee(1) here to achieve tight monitoring
						$code .= qq[tee \$SGDIR/nps-$c->{store_name}.use >\$SGDIR/nps-$c->{store_name}.monitor ) & SGPID="\$! \$SGPID"$monitor_pid\n];
						$written_pipe{"nps-$c->{store_name}.use"} = 1;
						$written_pipe{"nps-$c->{store_name}.monitor"} = 1;
						$code .= qq[${opt_p}sgsh-monitor <\$SGDIR/nps-$c->{store_name}.monitor | ${opt_p}sgsh-writeval -s \$SGDIR/mon-nps-$c->{store_name} & SGPID="\$! \$SGPID"\n];
						$read_pipe{"nps-$c->{store_name}.monitor"} = 1;
						$kvstores .= "{\$SGDIR/mon-nps-$c->{store_name}}";
						$monitor = "<\$SGDIR/nps-$c->{store_name}.use";
						$read_pipe{"nps-$c->{store_name}.use"} = 1;
					} else {
						$monitor = '';
					}
					$code .= qq{ ${opt_p}sgsh-writeval $c->{store_flags} -s \$SGDIR/$c->{store_name} $monitor & SGPID="\$! \$SGPID"\n};
					$kvstores .= "{\$SGDIR/$c->{store_name}}";
				}
			} else {
				internal_error("Tailless command");
			}
		}
		$cmd_n++;
	}
	return ($code, $kvstores);
}

# Substitute the streams and stores in the passed command
# with their underlying implementations and return the result
sub
substitute_streams_and_stores
{
	my ($command) = @_;

	# Substitute a cat /stream/... ... command of at least two streams with an sgsh-tee -f -I -i ... command
	# This avoids deadlocks by ensuring that all streams are opened when the concatenations starts
	if ($command =~ m|\bcat(\s+/stream/[\w.\-+=,]+){2,}|) {
		$command =~ s/\bcat\b/${opt_p}sgsh-tee -f -I/;
		$command =~ s|(/stream/[\w.\-+=,]+)|-i $1|g;
	}

	# Substitute /stream/... gather points with the corresponding named pipe
	while ($command =~ m|/stream/([\w.\-+=,]+)|) {
		my $file_name = $1;
		$command =~ s|/stream/$file_name|join(' ', map("\$SGDIR\/$_", @{$parallel_file_map{$file_name}}))|eg;
		for my $np (@{$parallel_file_map{$file_name}}) {
			$read_pipe{$np} = 1;
		}
	}

	if ($opt_S) {
		# Sequential code
		# Substitute store:name points with corresponding variable
		$command =~ s|store:(\w+)|echo \$\{$1\}|g;
	} else {
		# Substitute store:name points with corresponding invocation of sgsh-reaval
		$command =~ s|store:(\w+)|${opt_p}sgsh-readval -s \$SGDIR/$1|g;
	}
	return $command;
}

# Return gather code for the string passed as an argument
sub
gather_code
{
	my($commands) = @_;
	my $code;

	$code .= "# Gather the results\n";

	for my $c (@{$commands}) {
		$code .= substitute_streams_and_stores($c->{body});
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
	internal_error("unget_line past beginning") if ($line_number-- == 0);
}

# Return true if the specified command processes its arguments one-by-one
# Only a few commands are an exception to this rule
sub
sequential_command
{
	my($cmd) = @_;

	return 0 if ($cmd =~ m/\b(paste|comm|join)\b/);
	return 0 if ($cmd =~ m/\bsort\b.*-m/);
	return 1;
}

# Verify the code
# Ensure that all /stream files are used exactly once
# Ensure that all stores are used in the gather block
# Ensure that the scatter code implements a DAG
# Ensure that pass-through streams do not block other streams
sub
verify_code
{
	my($level, $scatter_command, $gather_commands) = @_;

	my $commands = $scatter_command->{scatter_commands};
	for my $c (@{$commands}) {
		my $body = $c->{body};
		chop $body;
		# Commands are executed asynchronously
		# According to our heuristic
		# a deadlock can occur if we use a pass-through stream in the same command as another
		# E.g. cat /stream/a /stream/b
		# where b is a pass-through stream
		my $processed_stream = 0;
		while ($body =~ s|/stream/([\w.\-+=,]+)||) {
			my $file_name = $1;
			error("Undefined stream /stream/$file_name specified for input", $c->{line_number}) unless ($defined_stream{$file_name});
			error("Stream /stream/$file_name used for input more than once", $c->{line_number}) if ($used_stream{$file_name});
			if ($edge{$parallel_graph_file_map{$file_name}[0]}->{teearg} && $processed_stream && sequential_command($body)) {
				warning("Unsafe use of pass-through /stream/$file_name in the scatter section", $c->{line_number});
				warning("Consult the DEADLOCK section of the manual page", $c->{line_number});
			}
			$used_stream{$file_name} = 1;
			$processed_stream = 1;
		}

		# Mark used stores
		while ($body =~ s|store:(\w+)||) {
			$used_store{$1} = 1;
		}

		if ($c->{output} eq 'scatter') {
			verify_code($level + 1, $c);
		}
	}

	my $processed_block_stream = 0;	# Executed synchronously; one stream can block all the rest
	for my $command (@{$gather_commands}) {
		my $tmp_command = $command->{body};
		my $processed_command_stream = 0;
		while ($tmp_command =~ s|/stream/([\w.\-+=,]+)||) {
			my $file_name = $1;
			error("Undefined stream /stream/$file_name specified for input", $command->{line_number}) unless ($defined_stream{$file_name});
			error("Stream /stream/$file_name used for input more than once", $command->{line_number}) if ($used_stream{$file_name});
			if ($edge{$parallel_graph_file_map{$file_name}[0]}->{teearg} &&
			    # Risk: we have already encountered a stream in a previous command in this block
			    ($processed_block_stream ||
			    # Risk: we have already encountered a stream in this unsafe command (e.g. cat)
			    ($processed_command_stream && sequential_command($tmp_command)))) {
				warning("Unsafe use of pass-through /stream/$file_name in the gather section", $command->{line_number});
				error("Consult the DEADLOCK section of the manual page", $command->{line_number});
			}
			$used_stream{$file_name} = 1;
			$processed_command_stream = 1;
		}
		$processed_block_stream = 1 if ($processed_command_stream);
		while ($tmp_command =~ s|store:(\w+)||) {
			$used_store{$1} = 1;
		}
	}

	if ($level == 0) {
		for my $gf (keys %defined_stream) {
			error("Stream /stream/$gf is never read") unless (defined($used_stream{$gf}));
		}
		for my $gv (keys %defined_store) {
			warning("Store store:$gv set but not used\n") unless (defined($used_store{$gv}));
		}

		my @cycle;
		if (defined($graph) && (@cycle = $graph->find_a_cycle())) {
			print STDERR "The following dependencies across streams form a cycle:\n";
			for my $node (@cycle) {
				print STDERR "$node_label{$node}\n";
			}
			exit 1;
		}
	}
}

# Verify code generation
sub
verify_generated_code
{
	for my $gp (keys %written_pipe) {
		internal_error("Named pipe $gp written-to but never read\n") unless (defined($read_pipe{$gp}));
	}
	for my $gp (keys %linked_pipe) {
		internal_error("Named pipe link $gp created but never read\n") unless (defined($read_pipe{$gp}));
	}
	for my $gp (keys %read_pipe) {
		internal_error("Named pipe $gp read but never written-to\n") unless (defined($written_pipe{$gp}) || defined($linked_pipe{$gp}));
	}
}

# Report an error and exit
sub
error
{
	my($message, $line) = @_;

	$line = $line_number unless(defined($line));
	print STDERR "$input_filename($line): error: $message\n";
	exit 1;
}

# Report an internal error and exit
sub
internal_error
{
	my($message, $line) = @_;

	$line = $line_number unless(defined($line));
	print STDERR "$input_filename($line): internal error: $message\n";
	exit 1;
}


# Report a warning (don't exit)
sub
warning
{
	my($message, $line) = @_;

	$line = $line_number unless(defined($line));
	print STDERR "$input_filename($line): warning: $message\n";
}

# Process the I/O redirection specifications returning
# the name of the tee node and a list of edges to which the specified command scatters the data
# setting the %edge and %parallel_graph_file_map
sub
scatter_graph_io
{
	my($command, $level, $parent_node_name) = @_;
	my @output_edges;

	# Generate graph heading
	print $graph_out qq[
	digraph \"\" {
		rankdir = LR;
		node $gv_node_attr;
		edge $gv_edge_attr;
	] if ($graph_out && $level == 0);

	my $scatter_n = $global_scatter_n++;

	my $show_tee = 0;

	my ($parallel, %scatter_opts) = parse_scatter_arguments($command);

	# Count number of scatter targets;
	my $commands = $command->{scatter_commands};

	my $scatter_targets = 0;
	for my $c (@{$commands}) {
		$scatter_targets++ if ($c->{input} eq 'scatter');
	}

	my $tee_node_name = "node_tee_$scatter_n";
	# Create tee, if needed
	if ($scatter_targets * $parallel >= 1) {
		# Create arguments
		my $tee_args = '';
		if ($scatter_opts{'s'}) {
			$tee_args .= ' -s';
			$show_tee = 1;
		}
		if ($scatter_opts{'l'}) {
			$tee_args .= ' -l';
			$show_tee = 1;
		}

		my $cmd_n = 0;
		for my $c (@{$commands}) {
			if ($c->{input} eq 'scatter') {
				for (my $p = 0; $p < $parallel; $p++) {
					$edge{"npi-$scatter_n.$cmd_n.$p"}->{lhs} = $tee_node_name;
				}
			}
			$cmd_n++;
		}

		if ($graph_out && ($show_tee || $level == 0)) {
			# Obtain tee program
			my $tee_prog = $opt_t;
			$tee_prog = $scatter_opts{'t'} if ($scatter_opts{'t'});
			$tee_prog =~ s|.*/||;

			print $graph_out qq{\t$tee_node_name [id="$tee_node_name", label="$tee_prog $tee_args", $gv_proc_attr];\n};
		}
	}

	# Process the commands
	# Pass 1: I/O redirection
	my $cmd_n = 0;
	for my $c (@{$commands}) {
		for (my $p = 0; $p < $parallel; $p++) {

			# Pass-through fast exit
			if ($c->{input} eq 'scatter' && $c->{body} eq '' && $c->{output} eq 'stream') {
				$edge{"npfo-$c->{file_name}.$p"}->{lhs} = $level == 0 ? $tee_node_name : $parent_node_name;
				$edge{"npfo-$c->{file_name}.$p"}->{teearg} = 1;
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
			} elsif ($c->{input} =~ m/fd([0-9]+)/) {
				;
			} else {
				internal_error("Headless command");
			}

			# Generate output redirection
			if ($c->{output} eq 'none') {
				;
			} elsif ($c->{output} eq 'scatter') {
				my ($tee_node_name, @output_edges) = scatter_graph_io($c, $level + 1, $node_name);
				# Generate scatter edges
				for my $output_edge (@output_edges) {
					$edge{$output_edge}->{lhs} =
						 ($#output_edges > 0 && $show_tee) ? $tee_node_name : $node_name;
				}
				# Connect our command to sgsh-tee
				print $graph_out qq{\t$node_name -> $tee_node_name\n} if ($graph_out && $#output_edges > 0 && $show_tee);
			} elsif ($c->{output} eq 'stream') {
				$edge{"npfo-$c->{file_name}.$p"}->{lhs} = $node_name;
				push(@{$parallel_graph_file_map{$c->{file_name}}}, "npfo-$c->{file_name}.$p");
			} elsif ($c->{output} eq 'store') {
				error("Store writes not allowed in parallel execution", $c->{line_number}) if ($p > 0);
				if ($graph_out) {
					print $graph_out qq(\t"$c->{store_name}" [id="store:$c->{store_name}", $gv_store_attr];\n);
					print $graph_out qq(\t$node_name -> "$c->{store_name}" [id="nps-$c->{store_name}"];\n)
				}
			} else {
				internal_error("Tailless command");
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
			next if ($c->{input} eq 'scatter' && $c->{body} eq '' && $c->{output} eq 'stream');

			my $node_name = "node_cmd_${scatter_n}_${cmd_n}_$p";

			# Generate body sans trailing newline and update corresponding edges
			my $body = $c->{body};
			chop $body;

			my $command_tmp = $body;
			# Generate stream input edges
			while ($command_tmp =~ s|/stream/([\w.\-+=,]+)||) {
				my $file_name = $1;
				for my $file_name_p (@{$parallel_graph_file_map{$file_name}}) {
					$edge{$file_name_p}->{rhs} = $node_name;
				}
			}

			# Generate store reading edges
			while ($command_tmp =~ s|\bstore:(\w+)||) {
				print $graph_out qq(\t$1 -> $node_name [id="store-read"];\n) if ($graph_out);
			}

			print $graph_out qq{\t$node_name [id="$node_name", label="} . graphviz_escape($body) . qq{", $gv_proc_attr];\n} if ($graph_out);
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

	$name =~ s/\s+$//;
	$name =~ s/^\s+//;
	if ($graph_style eq 'pretty') {
		# Attempt to remove HERE documents
		if ($name =~ m/<<\-?(.*?)\n/) {
			my $delim = $1;
			$delim =~ s/[\\\'\"]//g;
			# /s makes . match newlines
			$name =~ s/<<\-?(.*?)\n.*$delim//s;
		}

		# Remove single-quoted elements
		$name =~ s/'[^']*'//g;
		# Remove double-quoted elements
		$name =~ s/\\\"/QUOTE/g;
		$name =~ s/"[^"]*"//g;
		# Remove comments
		$name =~ s/\s*\#[^\n]*//g;
		# Remove empty lines
		$name =~ s/\n\n+/\n/g;
		# Escape special characters
		$name =~ s/\\/\\\\/g;
		# Remove arguments of first command
		$name =~ s/^([^ ]*) [^\n\|]*/$1/;
		# Remove arguments from subsequent commands
		$name =~ s/\n\s*([^ ]*)[^\n\|]*/\n$1/g;
		$name =~ s/\|[ \t]*([^ ]*)[^\n\|]*/|$1/g;
		# Improve presentation of pipes
		$name =~ s/\|/ | /g;
	} else {
		# Escape special characters
		$name =~ s/\\/\\\\/g;
		$name =~ s/"/\\"/g;
	}
	# Left-aligned line breaks
	$name =~ s/\n\s*/\\l/g;
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
		my $command_tmp = $command->{body};

		# Handle streams
		while ($command_tmp =~ s|/stream/([\w.\-+=,]+)||) {
			my $file_name = $1;
			for my $file_name_p (@{$parallel_graph_file_map{$file_name}}) {
				$edge{$file_name_p}->{rhs} = $node_name;
			}
			$is_node = 1;
		}

		# Handle stores
		while ($command_tmp =~ s|\bstore:(\w+)||) {
			print $graph_out qq(\t$1 -> $node_name [id="store-read"];\n) if ($graph_out);
			$is_node = 1;
		}

		print $graph_out qq{\t$node_name [id="$node_name", label="} . graphviz_escape($command->{body}) . qq{", $gv_proc_attr];\n} if ($graph_out && $is_node);
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
			print $graph_out qq{\tunknown_node_$u [label="???"]; // $e\n} if ($graph_out);
			$edge{$e}->{lhs} = "unknown_node_$u";
			$u++;
		}
		if (!defined($edge{$e}->{rhs})) {
			print $graph_out qq{\tunknown_node_$u [label="???"]; // $e\n} if ($graph_out);
			$edge{$e}->{rhs} = "unknown_node_$u";
			$u++;
		}
		$graph->add_edge($edge{$e}->{lhs}, $edge{$e}->{rhs}) if (defined($graph));
		print $graph_out qq|\t$edge{$e}->{lhs} -> $edge{$e}->{rhs} [id="$e"];\n| if ($graph_out);
	}
}

# Set the library path where the sgsh files reside
sub
set_library_path
{
	for my $p (('/usr/lib/sgsh', '/usr/local/lib/sgsh', $opt_p)) {
		if (-r "$p/graph-monitor.js") {
			$library_path = $p;
			last;
		}
	}

	if (!defined($library_path)) {
		print STDERR "Unable to locate Javascript resources.\n",
			     "Re-install sgsh or use -p flag\n";
		exit 1;
	}
}

# Write into the script commands to setup the debugging environment
# Called when setting up a scatter gather block
sub
debug_initialize
{
	return '' unless ($opt_d);

	set_library_path();

	# Copy HTML and Javascript files into current directory to
	# satisfy the browser's same-origin constraint for AJAX
	# requests
	($html_out, $html_filename)  = tempfile($temp_name, UNLINK => 1, SUFFIX => '.html');
	return qq{
cp "$library_path/jquery.js" \$SGDIR/
cp "$library_path/graph-monitor.css" \$SGDIR/
cp "$library_path/ajax-loader.gif" \$SGDIR/
cp "$html_filename" \$SGDIR/index.html
};
}

# Create the HTML file to monitor the script's behavior
sub
debug_create_html
{
	return unless ($opt_d);

	print $html_out q[<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <title>Processes and data flows - sgsh</title>
    <meta http-equiv="Content-type" content="text/html; charset=utf-8" />

    <link rel="stylesheet" type="text/css" href="graph-monitor.css" />

    <script src="jquery.js"></script>
    <script src="graph-monitor.js"></script>
  </head>
  <body id="body" >

    <p id="graph">
      <object height='900' type='image/svg+xml' width='500' id='thesvg'>
];
	# Append SVG
	close($graph_out);
	my ($svg_out, $svg_filename)  = tempfile($temp_name, UNLINK => 1, SUFFIX => '.svg');
	close($svg_out);
	system qq[dot -Tsvg <"$graph_filename" >>"$svg_filename"];
	if ($? == -1) {
		print STDERR "Unable to execute shell dot command: $!\n";
		exit 1;
	} elsif (($? >> 8) != 0) {
		$File::Temp::KEEP_ALL = 1;
		print STDERR "Error in generated dot file $graph_filename\n";
		exit 1;
	}

	# Copy generated SVG to HTML
	my %title;
	open(my $svg_in, '<', $svg_filename) || die "Unable to open $svg_filename: $!\n";
	while (<$svg_in>) {
		print $html_out $_;
	}

	# Finish-off HTML file
	print $html_out q[
</object>
    </p>

<!-- Hover popup for edges and store nodes -->
<div class="popup" id="pipePopup">
<table style="width:100%">
<tr><td class='colHead'>Bytes</td><td class='colValue'><span id="bytes"></span></td></tr>
<tr><td class='colHead'>Lines</td><td class='colValue'><span id="lines"></span></td></tr>
<tr><td class='colHead'>Bytes/s</td><td class='colValue'><span id="bps"></span></td></tr>
<tr><td class='colHead'>Lines/s</td><td class='colValue'><span id="lps"></span></td></tr>
<tr><td class='colHead'>Idle time</td><td class='colValue'><span id="idle"></span></td></tr>
<tr><th align='center' colspan=2>Last record</th></tr>
<tr><td colspan=2><span id="record"></span></td></tr>
</table>
</div>

<!-- Hover popup for processing nodes -->
<div class="popup" id="processPopup">
<table id="processData" style="width:100%">
</table>
</div>

<!-- Loading indicator -->
<img id="ajaxBusy" src="ajax-loader.gif" />

  </body>
</html>
];
}

# Return code to start a debug monitoring session
sub
debug_code
{
	return '' unless ($opt_d);

	return qq@
		# Start the web server in the directory with the stores
		( cd \$SGDIR ; ${opt_p}sgsh-httpval -n -m application/json -b 'server-bin/rusage?pid=%d:${opt_p}sgsh-ps %d' | ${opt_p}sgsh-writeval -s .SG_HTTP_PORT ) & SGPID="\$! \$SGPID"
		# Obtain the server's port
		SG_HTTP_PORT=`${opt_p}sgsh-readval -c -s \$SGDIR/.SG_HTTP_PORT`
		# Patch the Javascript with the correct port
		sed "s/HTTP_PORT/\$SG_HTTP_PORT/g" "$library_path/graph-monitor.js" >\$SGDIR/graph-monitor.js
		# Open the corresponding web page for the user
		for cmd in open cygstart gnome-open kde-open 'echo Point browser to '
		do
			if \$cmd http://localhost:\$SG_HTTP_PORT/index.html 2>/dev/null
			then
				break
			fi
		done
@
}
