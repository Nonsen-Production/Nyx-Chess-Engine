#pragma once

#include <cstdint>

// Piece base values
constexpr int PAWN_VALUE = 100;
constexpr int KNIGHT_VALUE = 320;
constexpr int BISHOP_VALUE = 330;
constexpr int ROOK_VALUE = 500;
constexpr int QUEEN_VALUE = 900;
constexpr int KING_VALUE = 20000;

struct Move {
  int from = -1;
  int to = -1;
  int promotion = 0;
  int movingType = 0;
  int capturedType = 0;
  bool isCapture = false;
  int orderScore = 0;
};
