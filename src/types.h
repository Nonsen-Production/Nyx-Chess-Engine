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

// Stack-allocated list of moves (max 256 per position is safe for chess)
struct MoveList {
  Move moves[256];
  int count = 0;

  void add(const Move &m) { moves[count++] = m; }
  Move &operator[](int i) { return moves[i]; }
  const Move &operator[](int i) const { return moves[i]; }
  int size() const { return count; }
  bool empty() const { return count == 0; }
  
  // For range-based for loops
  Move* begin() { return moves; }
  Move* end() { return moves + count; }
  const Move* begin() const { return moves; }
  const Move* end() const { return moves + count; }
};
