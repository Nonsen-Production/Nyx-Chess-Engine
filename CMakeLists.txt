cmake_minimum_required(VERSION 3.10)
project(NyxChessEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /GL /arch:AVX2 /DNDEBUG")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG")
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mcpu=apple-m1 -flto -DNDEBUG -funroll-loops -fno-exceptions")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto -DNDEBUG -funroll-loops -fno-exceptions")
endif()

find_package(Threads REQUIRED)

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

add_executable(nyx ${SOURCES})
target_link_libraries(nyx PRIVATE Threads::Threads)
target_include_directories(nyx PRIVATE src)