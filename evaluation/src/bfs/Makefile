# C compiler
CC = clang++
ICC = clang++
CC_FLAGS = -g -fopenmp -fopenmp-targets=nvptx64 -O2

all: bfs bfs_offload bfs_offload_ompdart bfs_offload_naive

bfs: bfs.cpp 
	$(CC) $(CC_FLAGS) bfs.cpp -o bfs 

bfs_offload: bfs.cpp
	$(ICC) $(CC_FLAGS) -DOMP_OFFLOAD bfs.cpp -o bfs_offload

bfs_offload_ompdart: bfs_ompdart_modified.cpp
	$(ICC) $(CC_FLAGS) -DOMP_OFFLOAD bfs_ompdart_modified.cpp -o bfs_offload_ompdart

bfs_offload_naive: bfs_naive.cpp
	$(ICC) $(CC_FLAGS) -DOMP_OFFLOAD bfs_naive.cpp -o bfs_offload_naive

clean:
	rm -f bfs bfs_offload bfs_offload_ompdart bfs_offload_naive result.txt
