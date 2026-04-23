#include "utils.h"
#include <cctype>

namespace Chess {

bool isInside(int square) { return square >= 0 && square < 64; }

int rowOf(int square) { return square / 8; }

int colOf(int square) { return square % 8; }

int squareFromAlgebraic(const std::string &square) {
  if (square.size() != 2)
    return -1;

  const int file = std::tolower(static_cast<unsigned char>(square[0])) - 'a';
  const int rank = square[1] - '1';

  if (file < 0 || file > 7 || rank < 0 || rank > 7)
    return -1;
  return rank * 8 + file;
}

std::string squareToAlgebraic(int square) {
  if (!isInside(square))
    return "??";

  std::string output;
  output.push_back(static_cast<char>('a' + colOf(square)));
  output.push_back(static_cast<char>('1' + rowOf(square)));
  return output;
}

int promotionPieceFromUciChar(char piece) {
  switch (std::tolower(static_cast<unsigned char>(piece))) {
  case 'q':
    return 5;
  case 'r':
    return 4;
  case 'b':
    return 3;
  case 'n':
    return 2;
  default:
    return 0;
  }
}

int pieceFromFenChar(char piece) {
  switch (piece) {
  case 'P':
    return 1;
  case 'N':
    return 2;
  case 'B':
    return 3;
  case 'R':
    return 4;
  case 'Q':
    return 5;
  case 'K':
    return 6;
  case 'p':
    return -1;
  case 'n':
    return -2;
  case 'b':
    return -3;
  case 'r':
    return -4;
  case 'q':
    return -5;
  case 'k':
    return -6;
  default:
    return 0;
  }
}

int pieceToIndex(int piece) {
  switch (piece) {
  case 1:
    return 0;
  case 2:
    return 1;
  case 3:
    return 2;
  case 4:
    return 3;
  case 5:
    return 4;
  case 6:
    return 5;
  case -1:
    return 6;
  case -2:
    return 7;
  case -3:
    return 8;
  case -4:
    return 9;
  case -5:
    return 10;
  case -6:
    return 11;
  default:
    return -1;
  }
}

} // namespace Chess
