#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "nnue.h"
#include "types.h"

struct Board {
  static constexpr int WHITE_KINGSIDE = 1 << 0;
  static constexpr int WHITE_QUEENSIDE = 1 << 1;
  static constexpr int BLACK_KINGSIDE = 1 << 2;
  static constexpr int BLACK_QUEENSIDE = 1 << 3;

  int sqaures[64] = {0};
  bool whiteTurn;
  int castle; // Bitmask [White Kingside, White Queenside, Black Kingside, Black Queenside]
  int moveNumber;
  int halfClock; // 50 move rule
  int enPassantSquare;
  std::array<std::uint64_t, 12> pieceBitboards = {0};
  std::uint64_t whitePieces = 0;
  std::uint64_t blackPieces = 0;
  std::uint64_t allPieces = 0;
  std::uint64_t zobristKey = 0;
  std::vector<std::uint64_t> positionHistory;
  NNUE::Accumulator nnueAccumulator;

  Board();
  void printBoard() const;
};

namespace Chess {
  void rebuildBitboards(Board &board);
  std::uint64_t positionHash(const Board &board);
  bool loadFEN(Board &board, const std::string &fen);
  bool loadPositionFromParts(Board &board, const std::string &boardPlacement,
                             const std::string &castlingRights,
                             const std::string &sideToMove);
} // namespace Chess
