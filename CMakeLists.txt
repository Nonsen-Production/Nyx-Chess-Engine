cmake_minimum_required(VERSION 3.10)
project(NyxChessEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add optimizations for release builds
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  # MSVC: multi-config generator — set Release flags directly
  # /O2 = maximize speed, /Oi = intrinsics, /GL = whole program optimization
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /GL /DNDEBUG")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG")
else()
  # GCC/Clang: single-config — set via CMAKE_CXX_FLAGS
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto -DNDEBUG")
  endif()
endif()

# Threading support for Lazy SMP
find_package(Threads REQUIRED)

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
    src/see.cpp
)

# Create the executable
add_executable(nyx ${SOURCES})

# Link threading library (Lazy SMP)
target_link_libraries(nyx PRIVATE Threads::Threads)

# Specify include directories so headers can be found
target_include_directories(nyx PRIVATE src)
