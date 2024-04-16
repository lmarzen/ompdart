#!/bin/bash

# Compiles and runs each version of the benchmark and profiles it with nsys

cd src

# bfs
echo "BUILDING BFS"
cd bfs
make clean
make -j
rm -rf results
mkdir results
echo "BENCHMARKING BFS"
echo "  PROFILING BASELINE"
nsys nvprof ./bfs_offload 4 ../../data/bfs/graph1MW_6.txt
echo "  PROFILING OMPDART"
nsys nvprof ./bfs_offload_ompdart 4 ../../data/bfs/graph1MW_6.txt
echo "  PROFILING NAIVE"
nsys nvprof ./bfs_offload_naive 4 ../../data/bfs/graph1MW_6.txt
cd ../

# hotspot
echo "BUILDING HOTSPOT"
cd hotspot
make clean
make -j
rm -rf results
mkdir results
echo "BENCHMARKING HOTSPOT"
echo "  PROFILING BASELINE"
nsys nvprof ./hotspot_offload 1024 1024 2 4 ../../data/hotspot/temp_1024 ../../data/hotspot/power_1024 results/output_baseline.out
echo "  PROFILING OMPDART"
nsys nvprof ./hotspot_offload_ompdart 1024 1024 2 4 ../../data/hotspot/temp_1024 ../../data/hotspot/power_1024 results/output_ompdart.out
echo "  PROFILING NAIVE"
nsys nvprof ./hotspot_offload_naive 1024 1024 2 4 ../../data/hotspot/temp_1024 ../../data/hotspot/power_1024 results/output_naive.out
cd ../

# nw
echo "BUILDING NW"
cd nw
make clean
make -j
rm -rf results
mkdir results
echo "BENCHMARKING NW"
echo "  PROFILING BASELINE"
nsys nvprof ./needle_offload 2048 10 2
echo "  PROFILING OMPDART"
nsys nvprof ./needle_offload_ompdart 2048 10 2
echo "  PROFILING NAIVE"
nsys nvprof ./needle_offload_naive 2048 10 2
cd ../

# accuracy
echo "BUILDING ACCURACY"
cd accuracy-omp
make -f Makefile clean
make -f Makefile.ompdart clean
make -f Makefile.naive clean
make -j -f Makefile
make -j -f Makefile.ompdart
make -j -f Makefile.naive
rm -rf results
mkdir results
echo "BENCHMARKING ACCURACY"
echo "  PROFILING BASELINE"
nsys nvprof /usr/bin/time -f "real %e" ./main 8192 10000 10 100
echo "  PROFILING OMPDART"
nsys nvprof /usr/bin/time -f "real %e" ./main_ompdart 8192 10000 10 100
echo "  PROFILING NAIVE"
nsys nvprof /usr/bin/time -f "real %e" ./main_naive 8192 10000 10 100
cd ../

# ace
echo "BUILDING ACE"
cd ace-omp
make -f Makefile clean
make -f Makefile.ompdart_aggressive clean
make -f Makefile.naive clean
make -j -f Makefile
make -j -f Makefile.ompdart_aggressive
make -j -f Makefile.naive
rm -rf results
mkdir results
echo "BENCHMARKING ACE"
echo "  PROFILING BASELINE"
nsys nvprof ./main 100
echo "  PROFILING OMPDART"
nsys nvprof ./main_ompdart_aggressive 100
echo "  PROFILING NAIVE"
nsys nvprof ./main_naive 100
cd ../

# backprop
echo "BUILDING BACKPROP"
cd backprop-omp
make -f Makefile clean
make -f Makefile.ompdart clean
make -f Makefile.naive clean
make -j -f Makefile
make -j -f Makefile.ompdart
make -j -f Makefile.naive
rm -rf results
mkdir results
echo "BENCHMARKING BACKPROP"
echo "  PROFILING BASELINE"
nsys nvprof ./main 65536
echo "  PROFILING OMPDART"
nsys nvprof ./main_ompdart 65536
echo "  PROFILING NAIVE"
nsys nvprof ./main_naive 65536
cd ../

# clenergy
echo "BUILDING CLENERGY"
cd clenergy-omp
make -f Makefile clean
make -f Makefile.ompdart clean
make -f Makefile.naive clean
make -j -f Makefile
make -j -f Makefile.ompdart
make -j -f Makefile.naive
rm -rf results
mkdir results
echo "BENCHMARKING CLENERGY"
echo "  PROFILING BASELINE"
nsys nvprof ./clenergy
echo "  PROFILING OMPDART"
nsys nvprof ./clenergy_ompdart
echo "  PROFILING NAIVE"
nsys nvprof ./clenergy_naive
cd ../

# xsbench
echo "BUILDING XSBENCH"
cd xsbench-omp
make -f Makefile clean
make -f Makefile.ompdart clean
make -f Makefile.naive clean
make -j -f Makefile
make -j -f Makefile.ompdart
make -j -f Makefile.naive
rm -rf results
mkdir results
echo "BENCHMARKING XSBENCH"
echo "  PROFILING BASELINE"
nsys nvprof ./XSBench -s large -m event -r 10
echo "  PROFILING OMPDART"
nsys nvprof ./XSBench_ompdart -s large -m event -r 10
echo "  PROFILING NAIVE"
nsys nvprof ./XSBench_naive -s large -m event -r 10
cd ../

# lulesh
echo "BUILDING LULESH"
cd lulesh-omp
make -f Makefile clean
make -f Makefile.ompdart clean
make -f Makefile.naive clean
make -j -f Makefile
make -j -f Makefile.ompdart
make -j -f Makefile.naive
rm -rf results
mkdir results
echo "BENCHMARKING LULESH"
echo "  PROFILING BASELINE"
nsys nvprof ./lulesh -i 100 -s 128 -r 11 -b 1 -c 1
echo "  PROFILING OMPDART"
nsys nvprof ./lulesh_ompdart -i 100 -s 128 -r 11 -b 1 -c 1
echo "  PROFILING NAIVE"
nsys nvprof ./lulesh_naive -i 100 -s 128 -r 11 -b 1 -c 1
cd ../


cd ../
