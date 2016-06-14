## Installation of unix tools adapted for sgsh

While under the sgsh root directory, type the following sequence of commands:

```bash
make negotiate    # compile the sgsh negotiation library
cd unix-sgsh-tools
make prepare      # download repos of Unix tools
make configure    # configure code
make adapt        # modify tools to use the sgsh form
make make         # compile
make install      # install in ./bin
make test         # run tests
```

The current collection of Unix tools include *coreutils, diffutils, gawk, grep, parallel, sed, and tar*.
Of those currently *comm* and *sort* have been adapted for use with sgsh.

## Adaptation workflow

The adapted form of tools are placed under unix-sgsh-tools/sgsh-adapted.

The workflow includes the following steps:

- copy the tool's source code from its original place to a proper place under sgsh-adapted and make a commit. e.g.

```bash
cp coreutils/src/sort.c sgsh-adapted/coreutils/sort.c
git add sgsh-adapted/coreutils/sort.c
git commit -a -m "Original code of sort tool"
```

- adapt the tool for use with sgsh
  - invoke the sgsh negotiation API similarly to the code that follows
  - use the provided file descriptors in place of the tool's original input and output interface. You can take a look at the patch files under *sgsh-adapted* to see examples.

```C
#include "sgsh-negotiate.h"

  int ninputfds = -1;
  int noutputfds = -1;
  int *inputfds;
  int *outputfds;
  char sgshin[10];
  char sgshout[11];
  int status = -1;

  if (!isatty(fileno(stdin))) strcpy(sgshin, "SGSH_IN=1");
  else strcpy(sgshin, "SGSH_IN=0");
  putenv(sgshin);
  if (!isatty(fileno(stdout))) strcpy(sgshout, "SGSH_OUT=1");
  else strcpy(sgshout, "SGSH_OUT=0");
  putenv(sgshout);

  if ((status = sgsh_negotiate("sort", -1, 1, &inputfds, &ninputfds,
                                            &outputfds, &noutputfds))) {
    printf("sgsh negotiation failed with status code %d.\n", status);
    exit(1);
  }

```

- produce patch from the original and adapted files
```bash
diff -u coreutils/src/sort.c sgsh-adapted/coreutils/sort.c > sgsh-adapted/coreutils/sort.patch
```
- modify the Makefile's *make adapt* recipe to patch the original file
```bash
patch -d coreutils/src sort.c sgsh-adapted/coreutils/sort.patch
```
- contribute tests
  - *cd simple_shell*
  - express a graph of sgsh adapted processes with interconnected inputs and outputs, such as the example that follows. The example has the comm and sort tools connect the former's output to the latter's input. Because they are both sgsh-compatible they use a socketpipe to carry out the sgsh negotiation phase. You can find this file as *comm_sort.sgsh*.
  - run the test to produce and store what a successful test output looks like. Name it *comm_sort.success*.

```bash
1 /home/mfg/dds/sgsh/unix-sgsh-tools/bin/comm f1s f2s
2 /home/mfg/dds/sgsh/unix-sgsh-tools/bin/sort

%
socketpipe 1 2
```

- add tests to Makefile's *make test* recipe

```Makefile
PSEUDOSHELLDIR=../simple-shell

test:
    cd $PSEUDOSHELLDIR && \
    python simple-shell.py comm_sort.sgsh && \
    sleep 1 && \
    echo "\nThe above output should match the following:" && \
    cat comm_sort.success
```

- run tests
```bash
make test
```