## The Directed Graph Shell (dgsh)

[![Build Status](https://travis-ci.org/dspinellis/dgsh.svg?branch=master)](https://travis-ci.org/dspinellis/dgsh)

The directed graph shell, *dgsh*, allows the expressive expression of efficient big data set and streams processing pipelines using existing Unix tools as well as custom-built components. It is a Unix-style shell allowing the specification of pipelines with non-linear scatter-gather operations. These form a directed acyclic process graph, which is typically executed by multiple processor cores, thus increasing the operation's processing throughput.

You can find a complete introduction, reference documentation,
and illustrated examples in the suite's
[web site](http://www.spinellis.gr/sw/dgsh/).

See also,
a [quick video overview](https://youtu.be/crqzO4YanwA) and
the associated (open access) paper,
[Extending Unix pipelines to DAGs](http://dx.doi.org/10.1109/TC.2017.2695447),
published in the *IEEE Transactions on Computers*, 66(9):1547–1561, 2017.
