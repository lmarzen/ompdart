# C compiler
CC = clang++
ICC = icc
CC_FLAGS = -g -fopenmp -O2 -Wno-writable-strings
OFFLOAD_CC_FLAGS = -fopenmp-targets=nvptx64

all: hotspot hotspot_offload hotspot_offload_ompdart hotspot_offload_naive


hotspot: hotspot_openmp.cpp Makefile 
	$(CC) $(CC_FLAGS) hotspot_openmp.cpp -o hotspot 

hotspot_offload: hotspot_openmp.cpp Makefile
	$(CC) $(CC_FLAGS) $(OFFLOAD_CC_FLAGS) -DOMP_OFFLOAD hotspot_openmp.cpp -o hotspot_offload

hotspot_offload_ompdart: hotspot_openmp_ompdart_modified.cpp Makefile
	$(CC) $(CC_FLAGS) $(OFFLOAD_CC_FLAGS) -DOMP_OFFLOAD hotspot_openmp_ompdart_modified.cpp -o hotspot_offload_ompdart

hotspot_offload_naive: hotspot_openmp_naive.cpp Makefile
	$(CC) $(CC_FLAGS) $(OFFLOAD_CC_FLAGS) -DOMP_OFFLOAD hotspot_openmp_naive.cpp -o hotspot_offload_naive

clean:
	rm -f hotspot hotspot_offload hotspot_offload_ompdart
