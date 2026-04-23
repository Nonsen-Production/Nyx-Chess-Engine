#pragma once

#include "board.h"
#include "types.h"
#include <vector>
#include <cstdint>

namespace Chess {

int findKing(const Board &board, bool white);
bool isPathClear(const Board &board, int from, int to);
bool isSquareAttacked(const Board &board, int square, bool byWhite);
bool isInCheck(const Board &board, bool white);

void clearCastlingRightForRookSquare(Board &board, int square);

bool makeMove(Board &board, int from, int to, int promotionPiece);
bool makeMove(Board &board, int from, int to);
bool applyMove(Board &board, const Move &move);

int popLeastSignificantBit(std::uint64_t &bitboard);

std::vector<Move> generateLegalMoves(const Board &board, bool capturesOnly);
bool hasAnyLegalMove(const Board &board, bool white);
int countLegalMoves(const Board &board, bool white);

bool isCheckmate(const Board &board, bool white);
bool isStalemate(const Board &board, bool white);
bool isFiftyMoveDraw(const Board &board);
bool isThreefoldRepetitionDraw(const Board &board);
bool isInsufficientMaterialDraw(const Board &board);
bool isDraw(const Board &board);

} // namespace Chess
