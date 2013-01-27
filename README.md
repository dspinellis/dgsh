A Unix-style shell allowing you the specification of pipelines with
non-linear scatter-gather operations.

This project is currently early-alpha quality.

# Syntax
```yacc
program : scatter '|{' command ... '}| gather |{' pipeline '|}'
        ;

command	: source pipeline sink
      	;

source	: '-|'          // Receive scatter input
      	| '.|'          // Receive no input
      	;

sink	: '|>/sgsh/' filename	        // Sink to specified file
    	| '|=' variable_name            // Sink to specified gather block variable
    	| '|{' command ... '|}'         // Scatter
        ;
```
