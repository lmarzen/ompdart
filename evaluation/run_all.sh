#!/bin/bash

# Compiles and runs each version of the benchmark.
# The execution time is averaged over N=30 runs.
N=30

cd src

# bfs
echo "BUILDING BFS"
cd bfs
make clean
make -j
rm -rf results
mkdir results
echo "BENCHMARKING BFS"
echo "  RUNNING BASELINE"
for i in $(seq $N); do ./bfs_offload 4 ../../data/bfs/graph1MW_6.txt ; done > results/results_baseline.out
cat results/results_baseline.out | grep "Compute time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR" (s)" }'
echo "  RUNNING OMPDART"
for i in $(seq $N); do ./bfs_offload_ompdart 4 ../../data/bfs/graph1MW_6.txt ; done > results/results_ompdart.out
cat results/results_ompdart.out | grep "Compute time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR" (s)" }'
echo "  RUNNING NAIVE"
for i in $(seq $N); do ./bfs_offload_naive 4 ../../data/bfs/graph1MW_6.txt ; done > results/results_naive.out
cat results/results_naive.out | grep "Compute time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR" (s)" }'
cd ../

# hotspot
echo "BUILDING HOTSPOT"
cd hotspot
make clean
make -j
rm -rf results
mkdir results
echo "BENCHMARKING HOTSPOT"
echo "  RUNNING BASELINE"
for i in $(seq $N); do ./hotspot_offload 1024 1024 2 4 ../../data/hotspot/temp_1024 ../../data/hotspot/power_1024 results/output_baseline.out ; done > results/results_baseline.out
cat results/results_baseline.out | grep "Total time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR" (s)" }'
echo "  RUNNING OMPDART"
for i in $(seq $N); do ./hotspot_offload_ompdart 1024 1024 2 4 ../../data/hotspot/temp_1024 ../../data/hotspot/power_1024 results/output_ompdart.out ; done > results/results_ompdart.out
cat results/results_ompdart.out | grep "Total time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR" (s)" }'
echo "  RUNNING NAIVE"
for i in $(seq $N); do ./hotspot_offload_naive 1024 1024 2 4 ../../data/hotspot/temp_1024 ../../data/hotspot/power_1024 results/output_naive.out ; done > results/results_naive.out
cat results/results_naive.out | grep "Total time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR" (s)" }'
cd ../

# nw
echo "BUILDING NW"
cd nw
make clean
make -j
rm -rf results
mkdir results
echo "BENCHMARKING NW"
echo "  RUNNING BASELINE"
> results/results_baseline.out && for i in $(seq $N); do ./needle_offload 2048 10 2 >> results/results_baseline.out ; done
cat results/results_baseline.out | grep "Total time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING OMPDART"
> results/results_ompdart.out && for i in $(seq $N); do ./needle_offload_ompdart 2048 10 2 >> results/results_ompdart.out ; done
cat results/results_ompdart.out | grep "Total time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING NAIVE"
> results/results_naive.out && for i in $(seq $N); do ./needle_offload_naive 2048 10 2 >> results/results_naive.out ; done
cat results/results_naive.out | grep "Total time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR " (s)" }'
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
echo "  RUNNING BASELINE"
for i in $(seq $N); do /usr/bin/time -f "real %e" ./main 8192 10000 10 100 ; done > results/results_baseline.out 2>&1
cat results/results_baseline.out | grep "real" | awk '{ sum += $2 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING OMPDART"
for i in $(seq $N); do /usr/bin/time -f "real %e" ./main_ompdart 8192 10000 10 100 ; done > results/results_ompdart.out 2>&1
cat results/results_ompdart.out | grep "real" | awk '{ sum += $2 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING NAIVE"
for i in $(seq $N); do /usr/bin/time -f "real %e" ./main_naive 8192 10000 10 100 ; done > results/results_naive.out 2>&1
cat results/results_naive.out | grep "real" | awk '{ sum += $2 } END { print "Average Exec Time = " sum/NR " (s)" }'
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
echo "  RUNNING BASELINE"
for i in $(seq $N); do ./main 100 ; done > results/results_baseline.out
cat results/results_baseline.out | grep "Offload time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR/1000 " (s)" }'
echo "  RUNNING OMPDART"
for i in $(seq $N); do ./main_ompdart_aggressive 100 ; done > results/results_ompdart.out
cat results/results_ompdart.out | grep "Offload time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR/1000 " (s)" }'
echo "  RUNNING NAIVE"
for i in $(seq $N); do ./main_naive 100 ; done > results/results_naive.out
cat results/results_naive.out | grep "Offload time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR/1000 " (s)" }'
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
echo "  RUNNING BASELINE"
for i in $(seq $N); do ./main 65536 ; done > results/results_baseline.out
cat results/results_baseline.out | grep "Device offloading time" | awk '{ sum += $5 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING OMPDART"
for i in $(seq $N); do ./main_ompdart 65536 ; done > results/results_ompdart.out
cat results/results_ompdart.out | grep "Device offloading time" | awk '{ sum += $5 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING NAIVE"
for i in $(seq $N); do ./main_naive 65536 ; done > results/results_naive.out
cat results/results_naive.out | grep "Device offloading time" | awk '{ sum += $5 } END { print "Average Exec Time = " sum/NR " (s)" }'
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
echo "  RUNNING BASELINE"
for i in $(seq $N); do ./clenergy ; done > results/results_baseline.out
cat results/results_baseline.out | grep "Total time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING OMPDART"
for i in $(seq $N); do ./clenergy_ompdart ; done > results/results_ompdart.out
cat results/results_ompdart.out | grep "Total time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING NAIVE"
for i in $(seq $N); do ./clenergy_naive ; done > results/results_naive.out
cat results/results_naive.out | grep "Total time:" | awk '{ sum += $3 } END { print "Average Exec Time = " sum/NR " (s)" }'
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
echo "  RUNNING BASELINE"
for i in $(seq $N); do ./XSBench -s large -m event -r 10 ; done > results/results_baseline.out
cat results/results_baseline.out | grep "Runtime:" | awk '{ sum += $2 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING OMPDART"
for i in $(seq $N); do ./XSBench_ompdart -s large -m event -r 10 ; done > results/results_ompdart.out
cat results/results_ompdart.out | grep "Runtime:" | awk '{ sum += $2 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING NAIVE"
for i in $(seq $N); do ./XSBench_naive -s large -m event -r 10 ; done > results/results_naive.out
cat results/results_naive.out | grep "Runtime:" | awk '{ sum += $2 } END { print "Average Exec Time = " sum/NR " (s)" }'
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
echo "  RUNNING BASELINE"
for i in $(seq $N); do ./lulesh -i 100 -s 128 -r 11 -b 1 -c 1 ; done > results/results_baseline.out
cat results/results_baseline.out | grep "Elapsed time" | awk '{ sum += $4 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING OMPDART"
for i in $(seq $N); do ./lulesh_ompdart -i 100 -s 128 -r 11 -b 1 -c 1 ; done > results/results_ompdart.out
cat results/results_ompdart.out | grep "Elapsed time" | awk '{ sum += $4 } END { print "Average Exec Time = " sum/NR " (s)" }'
echo "  RUNNING NAIVE"
for i in $(seq $N); do ./lulesh_naive -i 100 -s 128 -r 11 -b 1 -c 1 ; done > results/results_naive.out
cat results/results_naive.out | grep "Elapsed time" | awk '{ sum += $4 } END { print "Average Exec Time = " sum/NR " (s)" }'
cd ../


cd ../
