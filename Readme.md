# Shell Calc

Shell calc is a simple-to-use calculator program that runs in the shell. It provides easy access to a few common mathematical constants and functions and can quickly and easily be extended to add whatever constants or functions you'd like.

Shell calc was designed to be run as a **C script**, allowing for quick & easy modification of the source as needed without having to compile the code. In order to run Shell calc as a script, you must install TCC ([Tiny C Compiler](https://bellard.org/tcc/)). If you're running Linux, it may be in your package repository. For Debian/Ubuntu, simply run *`sudo apt-get install tcc`*. Alternatively, if you wish to compile the program instead, a small script is included that can compile this with either TCC or GCC. See the build & install instructions below.

 - [Usage](#usage)
 - [Constants and Functions](#constants-and-functions)
 - [Usage Examples](#usage-examples)
 - [Adding your own constants and functions](#adding-your-own-constants-and-functions)
 - [Installation](#installation)

### Usage:    

    dev@dev-laptop:~$ calc
    Usage: calc [-c -d] [expression]
    This is a simplistic expression calculator that's very easy to use from the shell.
    It can take values in Base 10, 16, or 8. It has some built in constants and
    functions, and one can easily add more functions or constants. Expression inputs
    are evaluated according to the order of operations: PE(MD)(AS).
    
            -d      Enable debug output
            -c      Print supported constants & functions
            -i      Input mode. Reads expression input from the terminal
    
    Supported operators:
    
            ^ - Exponent
            * - Multiply
            / - Divide
            % - Modulus
            + - Addition
            - - Subtraction


### Constants and Functions:
Here is the list of constants and functions that are currently built into Shell calc. Not impressed? See below for a demonstration on how to add your own.

    dev@dev-laptop:~$ calc -c
            pi      3.1415926536    The ratio of a circle's circumference to its diameter
            e       2.7182818285    Euler's number, base of the natural logarithm
    
            sin()   Sine function
            cos()   Cosine function
            sqrt()  Square-root function


### Usage Examples

Shell calc can be invoked with an expression as an argument. It will print out the result of the calculation and exit:

    dev@dev-laptop:~$ calc 'sqrt(5*5 + 6*6)'
    7.8102496759
    dev@dev-laptop:~$ calc 1*2*3*4*5*6*7*8*9
    Base 10: 362880
    Base 16: 58980
    dev@dev-laptop:~$ calc 'sin(0.5)+cos(37.5729/41.92)^4.5'
    0.5996266069

You can also run Shell calc in *input mode*:

    dev@dev-laptop:~/code/shell-calc$ calc -i
    Running in input mode. Type 'quit' or 'qq' to exit
    You can use up/down arrow keys to navigate expression history.
    Ctrl-C will clear the current input.
    
    Enter expression> 2048^2
    Base 10: 4194304
    Base 16: 400000
    Enter expression> (1/2048) ^ -2
    Base 10: 4194304
    Base 16: 400000
And as long as you don't overflow a double or int64, you can work with large numbers:

    Enter expression> 1024^4*8
    Base 10: 8796093022208
    Base 16: 0

## Adding your own constants and functions
This is one part of the code that needs improvement and will be changing very soon. However, right now there are two functions to update: `doFunc()` and `getConst()`. Just add your constant/variable to the correct function (it's pretty simple, just look at the code).

## Installation

If you intend to run this as a script, you'll need to install TCC. *`sudo apt-get install tcc`* should work on Debian/Ubuntu. If instead you want to compile it, just run the included compile script: `./compile.sh`The compile script uses GCC by default, but you can can uncoment a line in the script to compile with TCC instead.

In either case, to install Shell calc, just copy it to your local bin directory: `sudo cp calc.c /usr/local/bin/`
