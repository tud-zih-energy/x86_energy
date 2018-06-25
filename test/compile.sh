#!/bin/sh
gcc -I../src/include -L../Debug  -L/home/rschoene/tmp/x86_adapt/build -L/home/rschoene/install/lib test.c -o test -lscorep_new_x86_energy -llikwid -lx86_adapt -lm