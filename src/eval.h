#pragma once

#include "board.h"
#include <cstdint>

namespace Eval {

int mirrorSquare(int square);
int pieceBaseValue(int pieceType);
int mgPieceValue(int pieceType);
int egPieceValue(int pieceType);
int phaseWeight(int pieceType);
int pieceSquareBonus(int pieceType, int square, bool isWhitePiece);
int pieceSquareBonusEG(int pieceType, int square, bool isWhitePiece);

bool isPassedPawn(int square, bool whitePawn, std::uint64_t enemyPawns);
int evaluatePawnStructure(const Board &board, bool white);
int evaluateKingSafety(const Board &board, bool white, int phase);
int evaluateMobility(const Board &board, bool white);

int evaluateStaticHCE(const Board &board);
int evaluateStatic(const Board &board);
int evaluate(const Board &board);
double evaluateInPawns(const Board &board);

} // namespace Eval
