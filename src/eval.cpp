#include "eval.h"
#include "utils.h"
#include "movegen.h"
#include <algorithm>
#include <cmath>

namespace Eval {

int mirrorSquare(int square) {
  return (7 - Chess::rowOf(square)) * 8 + Chess::colOf(square);
}

int pieceBaseValue(int pieceType) {
  switch (pieceType) {
  case 1:
    return PAWN_VALUE;
  case 2:
    return KNIGHT_VALUE;
  case 3:
    return BISHOP_VALUE;
  case 4:
    return ROOK_VALUE;
  case 5:
    return QUEEN_VALUE;
  case 6:
    return KING_VALUE;
  default:
    return 0;
  }
}

int mgPieceValue(int pieceType) {
  switch (pieceType) {
  case 1:
    return 82;
  case 2:
    return 337;
  case 3:
    return 365;
  case 4:
    return 477;
  case 5:
    return 1025;
  default:
    return 0;
  }
}

int egPieceValue(int pieceType) {
  switch (pieceType) {
  case 1:
    return 94;
  case 2:
    return 281;
  case 3:
    return 297;
  case 4:
    return 512;
  case 5:
    return 936;
  default:
    return 0;
  }
}

int phaseWeight(int pieceType) {
  switch (pieceType) {
  case 2:
  case 3:
    return 1;
  case 4:
    return 2;
  case 5:
    return 4;
  default:
    return 0;
  }
}

int pieceSquareBonus(int pieceType, int square, bool isWhitePiece) {
  static const int pawnTable[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,  10, 10, 10, -10, -10, 10, 10, 10,
      5,  0,  0,  20, 20, 0,  0,  5,  0,  0,  5,  25,  25,  5,  0,  0,
      5,  5,  10, 30, 30, 10, 5,  5,  10, 10, 20, 35,  35,  20, 10, 10,
      40, 40, 40, 45, 45, 40, 40, 40, 0,  0,  0,  0,   0,   0,  0,  0};

  static const int knightTable[64] = {
      -50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,
      0,   -20, -40, -30, 0,   10,  15,  15,  10,  0,   -30, -30, 5,
      15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,
      -30, -30, 5,   10,  15,  15,  10,  5,   -30, -40, -20, 0,   5,
      5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};

  static const int bishopTable[64] = {
      -20, -10, -10, -10, -10, -10, -10, -20, -10, 5,   0,   0,   0,
      0,   5,   -10, -10, 10,  10,  10,  10,  10,  10,  -10, -10, 0,
      10,  15,  15,  10,  0,   -10, -10, 5,   5,   15,  15,  5,   5,
      -10, -10, 0,   5,   10,  10,  5,   0,   -10, -10, 0,   0,   0,
      0,   0,   0,   -10, -20, -10, -10, -10, -10, -10, -10, -20};

  static const int rookTable[64] = {
      0,  0,  5,  10, 10, 5,  0,  0,  -5, 0, 0, 5, 5, 0, 0, -5,
      -5, 0,  0,  5,  5,  0,  0,  -5, -5, 0, 0, 5, 5, 0, 0, -5,
      -5, 0,  0,  5,  5,  0,  0,  -5, -5, 0, 0, 5, 5, 0, 0, -5,
      5,  10, 10, 15, 15, 10, 10, 5,  0,  0, 0, 5, 5, 0, 0, 0};

  static const int queenTable[64] = {
      -20, -10, -10, -5,  -5,  -10, -10, -20, -10, 0,   0,   0,  0,
      0,   0,   -10, -10, 0,   5,   5,   5,   5,   0,   -10, -5, 0,
      5,   5,   5,   5,   0,   -5,  0,   0,   5,   5,   5,   5,  0,
      -5,  -10, 5,   5,   5,   5,   5,   0,   -10, -10, 0,   5,  0,
      0,   0,   0,   -10, -20, -10, -10, -5,  -5,  -10, -10, -20};

  static const int kingTable[64] = {
      -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50,
      -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40,
      -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30,
      -20, -10, -20, -20, -20, -20, -20, -20, -10, 20,  20,  0,   0,
      0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};

  const int tableIndex = isWhitePiece ? square : mirrorSquare(square);

  switch (pieceType) {
  case 1:
    return pawnTable[tableIndex];
  case 2:
    return knightTable[tableIndex];
  case 3:
    return bishopTable[tableIndex];
  case 4:
    return rookTable[tableIndex];
  case 5:
    return queenTable[tableIndex];
  case 6:
    return kingTable[tableIndex];
  default:
    return 0;
  }
}

int pieceSquareBonusEG(int pieceType, int square, bool isWhitePiece) {
  static const int kingEndgameTable[64] = {
      -50, -30, -30, -30, -30, -30, -30, -50, -30, -10, -10, -10, -10,
      -10, -10, -30, -30, -10, 20,  25,  25,  20,  -10, -30, -30, -10,
      25,  35,  35,  25,  -10, -30, -30, -10, 25,  35,  35,  25,  -10,
      -30, -30, -10, 20,  25,  25,  20,  -10, -30, -30, -20, -10, -10,
      -10, -10, -20, -30, -50, -40, -30, -20, -20, -30, -40, -50};

  if (pieceType == 6) {
    const int tableIndex = isWhitePiece ? square : mirrorSquare(square);
    return kingEndgameTable[tableIndex];
  }

  return pieceSquareBonus(pieceType, square, isWhitePiece) / 2;
}

static constexpr std::uint64_t FILE_MASKS[8] = {
    0x0101010101010101ULL, 0x0202020202020202ULL, 0x0404040404040404ULL,
    0x0808080808080808ULL, 0x1010101010101010ULL, 0x2020202020202020ULL,
    0x4040404040404040ULL, 0x8080808080808080ULL};

bool isPassedPawn(int square, bool whitePawn, std::uint64_t enemyPawns) {
  const int file = Chess::colOf(square);
  const int rank = Chess::rowOf(square);
  const int minFile = std::max(0, file - 1);
  const int maxFile = std::min(7, file + 1);

  if (whitePawn) {
    for (int r = rank + 1; r < 8; ++r) {
      for (int f = minFile; f <= maxFile; ++f) {
        if (enemyPawns & (1ULL << (r * 8 + f)))
          return false;
      }
    }
  } else {
    for (int r = rank - 1; r >= 0; --r) {
      for (int f = minFile; f <= maxFile; ++f) {
        if (enemyPawns & (1ULL << (r * 8 + f)))
          return false;
      }
    }
  }

  return true;
}

int evaluatePawnStructure(const Board &board, bool white) {
  const std::uint64_t pawns = board.pieceBitboards[white ? 0 : 6];
  const std::uint64_t enemyPawns = board.pieceBitboards[white ? 6 : 0];

  int score = 0;

  for (int file = 0; file < 8; ++file) {
    const int count = __builtin_popcountll(
        static_cast<unsigned long long>(pawns & FILE_MASKS[file]));
    if (count > 1) {
      score -= 14 * (count - 1); // doubled pawn penalty
    }
  }

  std::uint64_t iterator = pawns;
  static const int passedBonus[8] = {0, 5, 12, 22, 38, 60, 90, 0};

  while (iterator) {
    const int square =
        __builtin_ctzll(static_cast<unsigned long long>(iterator));
    iterator &= iterator - 1;

    const int file = Chess::colOf(square);
    const int rank = Chess::rowOf(square);

    std::uint64_t adjacentFiles = 0;
    if (file > 0)
      adjacentFiles |= FILE_MASKS[file - 1];
    if (file < 7)
      adjacentFiles |= FILE_MASKS[file + 1];

    if ((pawns & adjacentFiles) == 0) {
      score -= 12; // isolated pawn
    }

    if (isPassedPawn(square, white, enemyPawns)) {
      const int advance = white ? rank : (7 - rank);
      score += passedBonus[advance];
    }

    std::uint64_t supportMask = 0;
    if (file > 0) {
      if (rank > 0)
        supportMask |= 1ULL << ((rank - 1) * 8 + file - 1);
      if (rank < 7)
        supportMask |= 1ULL << ((rank + 1) * 8 + file - 1);
    }
    if (file < 7) {
      if (rank > 0)
        supportMask |= 1ULL << ((rank - 1) * 8 + file + 1);
      if (rank < 7)
        supportMask |= 1ULL << ((rank + 1) * 8 + file + 1);
    }

    if (pawns & supportMask) {
      score += 6; // connected pawn support
    }
  }

  return score;
}

int evaluateKingSafety(const Board &board, bool white, int phase) {
  const int kingSquare = Chess::findKing(board, white);
  if (kingSquare == -1)
    return 0;

  const int kingRank = Chess::rowOf(kingSquare);
  const int kingFile = Chess::colOf(kingSquare);
  const std::uint64_t friendlyPawns = board.pieceBitboards[white ? 0 : 6];
  int score = 0;

  // Only apply pawn shield / open file logic in middlegame
  if (phase > 6) {
    // Pawn shield: check 3 files around king, 1-2 ranks ahead
    const int shieldDir = white ? 1 : -1;
    for (int df = -1; df <= 1; ++df) {
      const int f = kingFile + df;
      if (f < 0 || f > 7)
        continue;

      bool foundShield = false;
      // Check 1 rank ahead
      const int r1 = kingRank + shieldDir;
      if (r1 >= 0 && r1 < 8 && (friendlyPawns & (1ULL << (r1 * 8 + f)))) {
        score += 10;
        foundShield = true;
      }
      // Check 2 ranks ahead
      if (!foundShield) {
        const int r2 = kingRank + 2 * shieldDir;
        if (r2 >= 0 && r2 < 8 && (friendlyPawns & (1ULL << (r2 * 8 + f)))) {
          score += 5;
          foundShield = true;
        }
      }
      if (!foundShield) {
        score -= 15; // No pawn shield on this file
      }
    }

    // Open file penalty: king file and adjacent files with no friendly pawns
    for (int df = -1; df <= 1; ++df) {
      const int f = kingFile + df;
      if (f < 0 || f > 7)
        continue;
      if ((friendlyPawns & FILE_MASKS[f]) == 0) {
        score -= 20;
      }
    }
  }

  // Castling bonus/penalty
  if (white) {
    // King on g1 or c1 = castled
    if (kingSquare == 6 || kingSquare == 2) {
      score += 30;
    } else if ((board.castle & (Board::WHITE_KINGSIDE | Board::WHITE_QUEENSIDE)) == 0 &&
               kingSquare == 4) {
      // Lost castling rights but king still on e1
      score -= 20;
    }
  } else {
    // King on g8 or c8 = castled
    if (kingSquare == 62 || kingSquare == 58) {
      score += 30;
    } else if ((board.castle & (Board::BLACK_KINGSIDE | Board::BLACK_QUEENSIDE)) == 0 &&
               kingSquare == 60) {
      score -= 20;
    }
  }

  // King tropism: penalize enemy pieces near king
  if (phase > 6) {
    const std::uint64_t enemyPieces = white ? board.blackPieces : board.whitePieces;
    std::uint64_t enemies = enemyPieces;
    while (enemies) {
      const int sq = __builtin_ctzll(static_cast<unsigned long long>(enemies));
      enemies &= enemies - 1;
      const int dr = std::abs(Chess::rowOf(sq) - kingRank);
      const int dc = std::abs(Chess::colOf(sq) - kingFile);
      const int dist = std::max(dr, dc); // Chebyshev distance
      if (dist <= 2) {
        const int enemyType = std::abs(board.sqaures[sq]);
        // Weight by piece type: queen near king is scariest
        if (enemyType == 5)
          score -= 12;
        else if (enemyType == 4)
          score -= 6;
        else if (enemyType == 2 || enemyType == 3)
          score -= 4;
      }
    }
  }

  // Scale king safety by game phase (more important in middlegame)
  return (score * phase) / 24;
}

int evaluateMobility(const Board &board, bool white) {
  int score = 0;
  const std::uint64_t friendlyOcc = white ? board.whitePieces : board.blackPieces;
  const std::uint64_t allOcc = board.allPieces;

  static const int bishopDirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  static const int rookDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  static const int knightOffsets[8][2] = {{2, 1}, {2, -1}, {-2, 1}, {-2, -1},
                                          {1, 2}, {1, -2}, {-1, 2}, {-1, -2}};

  // Center control bonus
  static const int centerSquares[4] = {27, 28, 35, 36}; // d4, e4, d5, e5
  const std::uint64_t friendlyPawns = board.pieceBitboards[white ? 0 : 6];
  for (int cs : centerSquares) {
    if (friendlyPawns & (1ULL << cs)) {
      score += 15; // Pawn on center
    }
  }

  std::uint64_t pieces = white ? board.whitePieces : board.blackPieces;
  while (pieces) {
    const int from = __builtin_ctzll(static_cast<unsigned long long>(pieces));
    pieces &= pieces - 1;

    const int piece = board.sqaures[from];
    const int pieceType = std::abs(piece);
    const int row = Chess::rowOf(from);
    const int col = Chess::colOf(from);
    int mobility = 0;

    switch (pieceType) {
    case 2: { // Knight
      for (const auto &offset : knightOffsets) {
        const int r = row + offset[0];
        const int c = col + offset[1];
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
          const int to = r * 8 + c;
          if (!(friendlyOcc & (1ULL << to))) {
            ++mobility;
            // Center control bonus for attacking center
            if (to == 27 || to == 28 || to == 35 || to == 36)
              score += 3;
          }
        }
      }
      score += mobility * 4;
      break;
    }
    case 3: { // Bishop
      for (const auto &dir : bishopDirs) {
        int r = row + dir[0];
        int c = col + dir[1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
          const int to = r * 8 + c;
          const std::uint64_t bit = 1ULL << to;
          if (friendlyOcc & bit)
            break;
          ++mobility;
          if (to == 27 || to == 28 || to == 35 || to == 36)
            score += 3;
          if (allOcc & bit)
            break;
          r += dir[0];
          c += dir[1];
        }
      }
      score += mobility * 3;
      break;
    }
    case 4: { // Rook
      for (const auto &dir : rookDirs) {
        int r = row + dir[0];
        int c = col + dir[1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
          const int to = r * 8 + c;
          const std::uint64_t bit = 1ULL << to;
          if (friendlyOcc & bit)
            break;
          ++mobility;
          if (allOcc & bit)
            break;
          r += dir[0];
          c += dir[1];
        }
      }
      score += mobility * 2;
      break;
    }
    case 5: { // Queen
      for (const auto &dir : bishopDirs) {
        int r = row + dir[0];
        int c = col + dir[1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
          const int to = r * 8 + c;
          const std::uint64_t bit = 1ULL << to;
          if (friendlyOcc & bit)
            break;
          ++mobility;
          if (allOcc & bit)
            break;
          r += dir[0];
          c += dir[1];
        }
      }
      for (const auto &dir : rookDirs) {
        int r = row + dir[0];
        int c = col + dir[1];
        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
          const int to = r * 8 + c;
          const std::uint64_t bit = 1ULL << to;
          if (friendlyOcc & bit)
            break;
          ++mobility;
          if (allOcc & bit)
            break;
          r += dir[0];
          c += dir[1];
        }
      }
      score += mobility * 1;
      break;
    }
    default:
      break;
    }
  }

  return score;
}

// Hand-crafted evaluation (fallback when NNUE is not loaded)
int evaluateStaticHCE(const Board &board) {
  int mgScore = 0;
  int egScore = 0;
  int phase = 0;
  int whiteBishops = 0;
  int blackBishops = 0;

  for (int square = 0; square < 64; ++square) {
    const int piece = board.sqaures[square];
    if (piece == 0)
      continue;

    const bool whitePiece = piece > 0;
    const int pieceType = std::abs(piece);
    const int sign = whitePiece ? 1 : -1;

    mgScore += sign * (mgPieceValue(pieceType) +
                       pieceSquareBonus(pieceType, square, whitePiece));
    egScore += sign * (egPieceValue(pieceType) +
                       pieceSquareBonusEG(pieceType, square, whitePiece));
    phase += phaseWeight(pieceType);

    if (pieceType == 3) {
      if (whitePiece)
        ++whiteBishops;
      else
        ++blackBishops;
    }
  }

  phase = std::min(24, phase);
  const int tapered = (mgScore * phase + egScore * (24 - phase)) / 24;

  int score = tapered;
  if (whiteBishops >= 2)
    score += 30;
  if (blackBishops >= 2)
    score -= 30;

  score += evaluatePawnStructure(board, true);
  score -= evaluatePawnStructure(board, false);

  score += evaluateKingSafety(board, true, phase);
  score -= evaluateKingSafety(board, false, phase);

  score += evaluateMobility(board, true);
  score -= evaluateMobility(board, false);

  if (Chess::isInCheck(board, false))
    score += 12;
  if (Chess::isInCheck(board, true))
    score -= 12;

  return score;
}

int evaluateStatic(const Board &board) {
  // Use NNUE evaluation when available
  if (NNUE::isReady() && board.nnueAccumulator.valid) {
    return NNUE::evaluate(board.nnueAccumulator, board.whiteTurn);
  }
  // Fallback to hand-crafted evaluation
  return evaluateStaticHCE(board);
}
int evaluate(const Board &board) {
  static constexpr int MATE_SCORE = 100000;

  if (Chess::isCheckmate(board, board.whiteTurn)) {
    return board.whiteTurn ? -MATE_SCORE : MATE_SCORE;
  }

  if (Chess::isDraw(board))
    return 0;

  return evaluateStatic(board);
}

double evaluateInPawns(const Board &board) {
  return static_cast<double>(evaluate(board)) / 100.0;
}

} // namespace Eval
