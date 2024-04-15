#===============================================================================
# User Options
#===============================================================================

# Compiler can be set below, or via environment variable
CC        = clang++
OPTIMIZE  = yes
DEBUG     = no
PROFILE   = no
MPI       = no
DEVICE    = gpu
LAUNCHER  =

#===============================================================================
# Program name & source code list
#===============================================================================

program = XSBench

obj = Main.o io.o Simulation.o GridInit.o XSutils.o Materials.o

#===============================================================================
# Sets Flags
#===============================================================================

# Standard Flags
CFLAGS := $(EXTRA_CFLAGS) -Wall
          
# Linker Flags
LDFLAGS = -lm

# Debug Flags
ifeq ($(DEBUG),yes)
  CFLAGS += -g
  LDFLAGS  += -g
endif

# Optimization Flags
ifeq ($(OPTIMIZE),yes)
  CFLAGS += -O3
endif

ifeq ($(DEVICE),gpu)
  CFLAGS +=-fopenmp -fopenmp-targets=nvptx64 -D__STRICT_ANSI__
else
  CFLAGS +=-fopenmp
endif

#===============================================================================
# Targets to Build
#===============================================================================

$(program): $(obj)
	$(CC) $(CFLAGS) $(obj) -o $@ $(LDFLAGS)

Main.o: Main.cu XSbench_header.h
	$(CC) -x c++ $(CFLAGS) -c $< -o $@

Simulation.o: Simulation.cpp XSbench_header.h
	$(CC) -x c++ $(CFLAGS) -c $< -o $@

io.o: io.cu XSbench_header.h
	$(CC) -x c++ $(CFLAGS) -c $< -o $@

GridInit.o: GridInit.cu XSbench_header.h
	$(CC) -x c++ $(CFLAGS) -c $< -o $@

Materials.o: Materials.cu XSbench_header.h
	$(CC) -x c++ $(CFLAGS) -c $< -o $@

XSutils.o: XSutils.cu XSbench_header.h
	$(CC) -x c++ $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(program) $(obj)

run: $(program)
	$(LAUNCHER) ./$(program) -s large -m event -r 10
