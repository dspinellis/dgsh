#!/usr/bin/env dgsh

row()
{
  dgsh -c "dgsh-parallel -n 5 'echo C{}' | paste"
}

matrix()
{
  dgsh -c "dgsh-parallel -n 5 row | cat"
}

export -f row matrix

call matrix | cat
