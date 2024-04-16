#!/bin/bash

# This script contains the commands used to generate the tool-generated
# mappings.
../run.sh -i src/accuracy-omp/main_stripped.cpp -o src/accuracy-omp/main_ompdart.cpp.new
../run.sh -i src/ace-omp/main_stripped.cpp -o src/ace-omp/main_ompdart_aggressive.cpp.new -a
../run.sh -i src/backprop-omp/main_stripped.cpp -o src/backprop-omp/main_ompdart.cpp.new
../run.sh -i src/bfs/bfs_stripped.cpp -o src/bfs/bfs_ompdart.cpp.new
../run.sh -i src/clenergy-omp/clenergy_stripped.cpp -o src/clenergy-omp/clenergy_ompdart.cpp.new
../run.sh -i src/hotspot/hotspot_openmp_stripped.cpp -o src/hotspot/hotspot_openmp_ompdart.cpp.new
../run.sh -i src/lulesh-omp/lulesh_stripped.cc -o src/lulesh-omp/lulesh_ompdart.cc.new
../run.sh -i src/nw/needle_stripped.cpp -o src/nw/needle_ompdart.cpp.new
../run.sh -i src/xsbench-omp/Simulation_stripped.cpp -o src/xsbench-omp/Simulation_ompdart.cpp.new

