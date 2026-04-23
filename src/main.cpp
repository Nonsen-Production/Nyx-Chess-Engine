/*
 * Nyx Chess Engine
 * A cross-platform UCI-compatible chess engine written in C++17
 *
 * Supported OS: Windows, macOS, Linux
 *
 * Build:  mkdir build && cd build && cmake .. && cmake --build . --config Release
 * Usage:  ./nyx (Linux/macOS) or nyx.exe (Windows)
 *         Place nn-nyx.nnue in working directory for NNUE eval
 *
 * License: MIT
 */

#include "nnue.h"
#include "book.h"
#include "uci.h"
#include <string>

std::string gGameMovesUCI;

int main() {
  // Initialize NNUE neural network
  NNUE::init("nn-nyx.nnue");

  // Initialize opening book
  Book::init();

  // Run the Universal Chess Interface loop
  uciLoop();

  return 0;
}