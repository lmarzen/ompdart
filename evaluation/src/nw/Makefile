# C compiler
CC = clang++
ICC = clang++
CC_FLAGS = -g -O3 -fopenmp -Wno-unused-comparison
OFFLOAD_CC_FLAGS = -fopenmp-targets=nvptx64

all: needle needle_offload needle_offload_ompdart needle_offload_naive

needle: 
	$(CC) $(CC_FLAGS) needle.cpp -o needle

needle_offload:
	$(ICC) $(CC_FLAGS) $(OFFLOAD_CC_FLAGS) -DOMP_OFFLOAD needle.cpp -o needle_offload

needle_offload_ompdart:
	$(ICC) $(CC_FLAGS) $(OFFLOAD_CC_FLAGS) -DOMP_OFFLOAD needle_ompdart_modified.cpp -o needle_offload_ompdart

needle_offload_naive:
	$(ICC) $(CC_FLAGS) $(OFFLOAD_CC_FLAGS) -DOMP_OFFLOAD needle_naive.cpp -o needle_offload_naive

clean:
	rm -f needle needle_offload needle_offload_ompdart needle_offload_naive
