#!/bin/sh

echo "Compiling calc.c..."

# Uncomment the below line to compile with tcc instead of gcc
# tcc -lm -On -o ./calc ./calc.c

# Compile with GCC
tail -n +3 ./calc.c | gcc -O0 -g3 -x c -o ./calc - -lm

if [ $? -eq 0 ]; then echo "Finished compiling. Executable file saved to ./calc. Enjoy!"; fi
