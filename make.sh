#!/bin/sh
set -eCx
gcc -Wall -Wextra -pedantic -O2 -o number_table number_table.c
strip -s number_table
