cmake_minimum_required(VERSION 3.10)
project(NyxChessEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add optimizations for release builds
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
endif()

# Gather source files from the src directory
set(SOURCES
    src/main.cpp
    src/board.cpp
    src/utils.cpp
    src/movegen.cpp
    src/eval.cpp
    src/search.cpp
    src/uci.cpp
    src/nnue.cpp
    src/book.cpp
    src/bitboard.cpp
)

# Create the executable
add_executable(nyx ${SOURCES})

# Specify include directories so headers can be found
target_include_directories(nyx PRIVATE src)
