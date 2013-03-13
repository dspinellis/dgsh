The scatter gather shell, *sgsh*, allows the efficient processing of
big data sets and streams using existing Unix tools as well as
custom-built components.
It is a Unix-style shell allowing the specification of pipelines with
non-linear scatter-gather operations.
These form a directed acyclic process graph, which is typically executed
by multiple processor cores,
thus increasing the operation's processing throughput.

*Sgsh* provides three inter-process communication mechanisms.

1. Scatter blocks send the output of one pipeline ending
with ```|{``` into multiple pipelines beginning with ```-|```.
The scatter block is terminated by ```|}```.
Scatter interconnections are internally implemented through
automatically-named pipes, and a program, ```sgsh-tee```, that distributes
the data to multiple processes.
1. Stores, named as ```store:```*name*, allow the storage of a data stream's
last record (or of a specified window of records) into a named buffer.
This record can be later retrieved asynchronously by one or more readers.
Data can be piped into a store or out of a store, or it can be read
using the shell's command output substitution syntax.
Stores are implemented internally through Unix-domain sockets,
a writer program, ```sgsh-write```, and a reader program, ```sgsh-read```.
1. Streams, named as ```/stream/```*name*, connect the output of one process
with the input of another.
In contrast to scatter blocks,
which restrict the data flow through a tree of processes,
streams allow the specification of a directed acyclic process graph.
Streams require exactly one reader and one writer in order to operate.
Consequently, they should not be used in sequential synchronous steps
specified in the gather block, because steps waiting to be executed
will completely block all upstream processing in the scatter block.
Streams are internally implemented through named pipes.


This project is currently beta quality.

# Syntax
```yacc
program : 'scatter |{' command ... '}| gather |{' pipeline '|}'
        ;

command	: source pipeline sink
      	;

source	: '-|'          // Receive scatter input
      	| '.|'          // Receive no input
      	;

sink	: '|>/stream/' filename	        // Sink to specified stream
    	| '|store:' store_name          // Sink to specified store
    	| '|{' command ... '|}'         // Scatter
        ;
```
