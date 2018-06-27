#!/bin/sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../_build

gdb ./test
