DIR=$(dirname "$0")
LIB="$DIR/build/src/rose/librose.so"
PLUGIN="-rose"
FILENAME=$1;

set -x
clang -Xclang -load -Xclang $LIB -Xclang -plugin -Xclang $PLUGIN -fopenmp -c $FILENAME
# gdb --args clang -Xclang -load -Xclang $LIB -Xclang -plugin -Xclang $PLUGIN -fopenmp -c $FILENAME
