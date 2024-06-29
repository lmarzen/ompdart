cmake_minimum_required(VERSION 3.20.0)
project(rose)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
find_package(Clang REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})

find_program(CLANG   clang   NO_DEFAULT_PATH PATHS "${LLVM_TOOLS_BINARY_DIR}")
find_program(CLANGXX clang++ NO_DEFAULT_PATH PATHS "${LLVM_TOOLS_BINARY_DIR}")
set(CMAKE_C_COMPILER "${CLANG}")
set(CMAKE_CXX_COMPILER "${CLANGXX}")

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Use the same C++ standard as LLVM
set(CMAKE_CXX_STANDARD 17 CACHE STRING "")
set(CMAKE_CXX_FLAGS "-Wall ${CMAKE_CXX_FLAGS}")

# LLVM/Clang is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_C_FLAGS   "-fcolor-diagnostics ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "-fcolor-diagnostics ${CMAKE_CXX_FLAGS}")
endif()

# If LLVM was build in debug mode, be consistent with that.
if ("${LLVM_ENABLE_ASSERTIONS}" STREQUAL "ON")
  set(CMAKE_C_FLAGS   "-g ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "-g ${CMAKE_CXX_FLAGS}")
else()
  set(CMAKE_C_FLAGS   "-DNDEBUG ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "-DNDEBUG ${CMAKE_CXX_FLAGS}")
endif()

add_subdirectory(src)
