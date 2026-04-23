#pragma once

#include <string>

namespace Chess {

bool isInside(int square);
int rowOf(int square);
int colOf(int square);

int squareFromAlgebraic(const std::string &square);
std::string squareToAlgebraic(int square);

int promotionPieceFromUciChar(char piece);
int pieceFromFenChar(char piece);
int pieceToIndex(int piece);

} // namespace Chess
