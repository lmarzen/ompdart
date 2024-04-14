DIR=$(dirname "$0")
LIB="$DIR/build/src/libompdart.so"
PLUGIN="ompdart"
INFILENAME=$1
OUTFILENAME=$2

set -x
# help
# clang -Xclang -load -Xclang $LIB -Xclang -plugin -Xclang $PLUGIN -Xclang -plugin-arg-$PLUGIN -Xclang --help -fopenmp -c $FILENAME
clang -Xclang -load -Xclang $LIB -Xclang -plugin -Xclang $PLUGIN -Xclang -plugin-arg-$PLUGIN -Xclang --output -Xclang -plugin-arg-$PLUGIN -Xclang "$OUTFILENAME" -fopenmp -c $INFILENAME
# gdb --args clang -Xclang -load -Xclang $LIB -Xclang -plugin -Xclang $PLUGIN -fopenmp -c $FILENAME
