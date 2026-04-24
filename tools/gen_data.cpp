/*
 * NNUE Training Data Generator for Nyx Chess Engine
 *
 * Generates training data by running random self-play games,
 * evaluating positions with the HCE, and writing (position, eval) pairs
 * in a format the PyTorch trainer can consume.
 *
 * Build: c++ -std=c++17 -O2 -I src -o gen_data tools/gen_data.cpp \
 *        src/movegen.cpp src/eval.cpp src/board.cpp src/nnue.cpp \
 *        src/utils.cpp src/bitboard.cpp src/book.cpp src/search.cpp
 *
 * Usage: ./gen_data [num_games] [output_file]
 *        ./gen_data 10000 train_data.bin
 *
 * Output format (per position):
 *   768 bytes: feature vector (0 or 1 for each input feature)
 *   4 bytes:   HCE eval (int32_t, from white's perspective, centipawns)
 *   1 byte:    side to move (0=white, 1=black)
 */

#include "board.h"
#include "movegen.h"
#include "eval.h"
#include "bitboard.h"
#include "nnue.h"

#include <iostream>
#include <random>
#include <cstdint>

// Write a position's features + eval to the output file
static void writePosition(std::ofstream &out, const Board &board) {
  // Feature vector: 768 bytes, one per input feature
  std::uint8_t features[768] = {};

  for (int sq = 0; sq < 64; ++sq) {
    int piece = board.sqaures[sq];
    if (piece == 0) continue;

    bool whitePiece = piece > 0;
    int pieceType = std::abs(piece);
    int typeIdx = pieceType - 1; // 0-5

    // White perspective feature
    int wColorOff = whitePiece ? 0 : 384;
    int wIdx = wColorOff + typeIdx * 64 + sq;
    features[wIdx] = 1;
  }

  // HCE eval from white's perspective
  // Temporarily disable NNUE so we use HCE
  std::int32_t eval = static_cast<std::int32_t>(Eval::evaluateStatic(board));
  std::uint8_t stm = board.whiteTurn ? 0 : 1;

  out.write(reinterpret_cast<const char *>(features), 768);
  out.write(reinterpret_cast<const char *>(&eval), 4);
  out.write(reinterpret_cast<const char *>(&stm), 1);
}

int main(int argc, char *argv[]) {
  int numGames = 10000;
  std::string outputFile = "train_data.bin";

  if (argc >= 2) numGames = std::atoi(argv[1]);
  if (argc >= 3) outputFile = argv[2];

  Bitboards::init();

  std::ofstream out(outputFile, std::ios::binary);
  if (!out.is_open()) {
    std::cerr << "Cannot create " << outputFile << std::endl;
    return 1;
  }

  std::mt19937 rng(42);
  int totalPositions = 0;

  for (int game = 0; game < numGames; ++game) {
    Board board;
    int moveCount = 0;

    while (moveCount < 200) {
      auto moves = Chess::generateLegalMoves(board, false);
      if (moves.empty()) break;

      // Skip first 4 moves (opening) for more diverse positions
      if (moveCount >= 8) {
        // Don't record positions where king is in check
        if (!Chess::isInCheck(board, board.whiteTurn)) {
          writePosition(out, board);
          ++totalPositions;
        }
      }

      // Choose a move — mix random (exploration) and top moves
      int moveIdx;
      if (rng() % 4 == 0) {
        // 25% random for diversity
        moveIdx = rng() % moves.size();
      } else {
        // 75% pick from top 3 moves by MVV-LVA heuristic
        moveIdx = rng() % std::min(static_cast<int>(moves.size()), 3);
      }

      Board next = board;
      if (Chess::applyMove(next, moves[moveIdx])) {
        board = next;
      }
      ++moveCount;
    }

    if ((game + 1) % 1000 == 0) {
      std::cout << "Games: " << (game + 1) << " / " << numGames
                << "  Positions: " << totalPositions << std::endl;
    }
  }

  out.close();
  std::cout << "Generated " << totalPositions << " positions to " << outputFile
            << std::endl;

  return 0;
}
