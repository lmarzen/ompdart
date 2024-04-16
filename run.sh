#!/bin/bash

DIR=$(dirname "$0")
LIB="$DIR/build/src/libompdart.so"
PLUGIN="ompdart"

CFLAGS=-fopenmp
COMMAND="clang $CFLAGS -Xclang -load -Xclang $LIB -Xclang -plugin -Xclang $PLUGIN"

while [ "$1" != "" ]; do
    case $1 in
        -h | --help )
            shift;
	    COMMAND="$COMMAND -Xclang -plugin-arg-$PLUGIN -Xclang --help"
            ;;

        -i | --input)
            shift;
	    COMMAND="$COMMAND -c $1"
            shift;
            ;;

        -o | --output)
            shift;
	    COMMAND="$COMMAND -Xclang -plugin-arg-$PLUGIN -Xclang --output -Xclang -plugin-arg-$PLUGIN -Xclang $1"
            shift;
            ;;

        -a | --aggresive-cross-function)
            shift;
	    COMMAND="$COMMAND -Xclang -plugin-arg-$PLUGIN -Xclang -a"
            shift;
            ;;
	    
        -d | --debug)
            shift;
	    COMMAND="gdb --args $COMMAND"
            ;;
        * )
            break;
            ;;
    esac
done

INFILENAME=$1
OUTFILENAME=$2

set -x
$COMMAND
# help
# clang -Xclang -load -Xclang $LIB -Xclang -plugin -Xclang $PLUGIN -Xclang -plugin-arg-$PLUGIN -Xclang --help -fopenmp -c $FILENAME
# clang -Xclang -load -Xclang $LIB -Xclang -plugin -Xclang $PLUGIN -Xclang -plugin-arg-$PLUGIN -Xclang --output -Xclang -plugin-arg-$PLUGIN -Xclang "$OUTFILENAME" -fopenmp -c $INFILENAME
# gdb --args clang -Xclang -load -Xclang $LIB -Xclang -plugin -Xclang $PLUGIN -fopenmp -c $FILENAME
