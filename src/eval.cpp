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
  const std::uint64_t enemyPawns = board.pieceBitboards[white ? 6 : 0];
  const std::uint64_t enemyPieces = white ? board.blackPieces : board.whitePieces;
  int score = 0;
  int attackerCount = 0;
  int attackWeight = 0;

  // === Pawn Shield (middlegame only) ===
  if (phase > 6) {
    const int shieldDir = white ? 1 : -1;
    for (int df = -1; df <= 1; ++df) {
      const int f = kingFile + df;
      if (f < 0 || f > 7)
        continue;

      bool foundShield = false;
      const int r1 = kingRank + shieldDir;
      if (r1 >= 0 && r1 < 8 && (friendlyPawns & (1ULL << (r1 * 8 + f)))) {
        score += (df == 0) ? 15 : 10; // Center file pawn is more important
        foundShield = true;
      }
      if (!foundShield) {
        const int r2 = kingRank + 2 * shieldDir;
        if (r2 >= 0 && r2 < 8 && (friendlyPawns & (1ULL << (r2 * 8 + f)))) {
          score += 4;
          foundShield = true;
        }
      }
      if (!foundShield) {
        score -= (df == 0) ? 25 : 18; // Missing shield on king file is worst
      }
    }

    // === Open / Semi-open file penalty near king ===
    for (int df = -1; df <= 1; ++df) {
      const int f = kingFile + df;
      if (f < 0 || f > 7)
        continue;
      bool hasFriendlyPawn = (friendlyPawns & FILE_MASKS[f]) != 0;
      bool hasEnemyPawn = (enemyPawns & FILE_MASKS[f]) != 0;
      if (!hasFriendlyPawn && !hasEnemyPawn) {
        score -= 25; // Fully open file
      } else if (!hasFriendlyPawn) {
        score -= 15; // Semi-open (enemy pawn only)
      }
    }

    // === Pawn Storm Detection ===
    // Enemy pawns advancing toward our king
    for (int df = -1; df <= 1; ++df) {
      const int f = kingFile + df;
      if (f < 0 || f > 7) continue;
      std::uint64_t filePawns = enemyPawns & FILE_MASKS[f];
      while (filePawns) {
        const int sq = __builtin_ctzll(static_cast<unsigned long long>(filePawns));
        filePawns &= filePawns - 1;
        const int pawnRank = Chess::rowOf(sq);
        int dist = white ? (pawnRank - kingRank) : (kingRank - pawnRank);
        // Negative dist means pawn is behind the king (no threat)
        if (dist >= 0 && dist <= 3) {
          score -= (4 - dist) * 6; // Closer = more dangerous
        }
      }
    }

    // === Attacker Counting (key king safety concept) ===
    // Count enemy pieces attacking the king zone (king + surrounding 8 squares)
    static const int kingZoneDirs[9][2] = {
        {0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 1},
        {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};

    std::uint64_t kingZone = 0;
    for (const auto &d : kingZoneDirs) {
      int r = kingRank + d[0];
      int c = kingFile + d[1];
      if (r >= 0 && r < 8 && c >= 0 && c < 8) {
        kingZone |= 1ULL << (r * 8 + c);
      }
    }

    // Check each enemy piece for attacks on king zone
    std::uint64_t enemies = enemyPieces;
    while (enemies) {
      const int sq = __builtin_ctzll(static_cast<unsigned long long>(enemies));
      enemies &= enemies - 1;
      const int enemyType = std::abs(board.sqaures[sq]);
      const int er = Chess::rowOf(sq);
      const int ec = Chess::colOf(sq);

      bool attacksKingZone = false;

      switch (enemyType) {
      case 2: { // Knight
        static const int knightOff[8][2] = {{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
        for (const auto &off : knightOff) {
          int r = er + off[0], c = ec + off[1];
          if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            if (kingZone & (1ULL << (r * 8 + c))) { attacksKingZone = true; break; }
          }
        }
        if (attacksKingZone) { ++attackerCount; attackWeight += 20; }
        break;
      }
      case 3: { // Bishop
        static const int bDirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
        for (const auto &dir : bDirs) {
          int r = er + dir[0], c = ec + dir[1];
          while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int to = r * 8 + c;
            if (kingZone & (1ULL << to)) { attacksKingZone = true; break; }
            if (board.allPieces & (1ULL << to)) break;
            r += dir[0]; c += dir[1];
          }
          if (attacksKingZone) break;
        }
        if (attacksKingZone) { ++attackerCount; attackWeight += 20; }
        break;
      }
      case 4: { // Rook
        static const int rDirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (const auto &dir : rDirs) {
          int r = er + dir[0], c = ec + dir[1];
          while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int to = r * 8 + c;
            if (kingZone & (1ULL << to)) { attacksKingZone = true; break; }
            if (board.allPieces & (1ULL << to)) break;
            r += dir[0]; c += dir[1];
          }
          if (attacksKingZone) break;
        }
        if (attacksKingZone) { ++attackerCount; attackWeight += 40; }
        break;
      }
      case 5: { // Queen
        static const int qDirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
        for (const auto &dir : qDirs) {
          int r = er + dir[0], c = ec + dir[1];
          while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            int to = r * 8 + c;
            if (kingZone & (1ULL << to)) { attacksKingZone = true; break; }
            if (board.allPieces & (1ULL << to)) break;
            r += dir[0]; c += dir[1];
          }
          if (attacksKingZone) break;
        }
        if (attacksKingZone) { ++attackerCount; attackWeight += 80; }
        break;
      }
      default: break;
      }
    }

    // Apply attacker penalty with quadratic scaling
    // More attackers = disproportionately more dangerous
    static const int safetyTable[9] = {0, 0, 50, 75, 88, 94, 97, 99, 100};
    int attackIndex = std::min(attackerCount, 8);
    score -= (attackWeight * safetyTable[attackIndex]) / 100;

    // === Virtual King Mobility ===
    // Count safe squares around the king (fewer = more dangerous)
    int safeSq = 0;
    for (int dr = -1; dr <= 1; ++dr) {
      for (int dc = -1; dc <= 1; ++dc) {
        if (dr == 0 && dc == 0) continue;
        int r = kingRank + dr, c = kingFile + dc;
        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
          int to = r * 8 + c;
          // Square is "safe" if not attacked by opponent and not blocked by friendly piece
          if (!Chess::isSquareAttacked(board, to, !white)) {
            ++safeSq;
          }
        }
      }
    }
    if (safeSq <= 1) score -= 50;
    else if (safeSq <= 2) score -= 25;
    else if (safeSq <= 3) score -= 10;
  }

  // === Castling bonus/penalty ===
  if (white) {
    if (kingSquare == 6 || kingSquare == 2) {
      score += 35;
    } else if ((board.castle & (Board::WHITE_KINGSIDE | Board::WHITE_QUEENSIDE)) == 0 &&
               kingSquare == 4) {
      score -= 25;
    }
  } else {
    if (kingSquare == 62 || kingSquare == 58) {
      score += 35;
    } else if ((board.castle & (Board::BLACK_KINGSIDE | Board::BLACK_QUEENSIDE)) == 0 &&
               kingSquare == 60) {
      score -= 25;
    }
  }

  // Scale king safety by game phase
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

// === Endgame Knowledge ===
// Adjusts the evaluation score based on material configuration and endgame patterns.
int evaluateEndgame(const Board &board, int score, int phase) {
  // Only apply in late middlegame / endgame (phase <= 10)
  if (phase > 10) return score;

  // Count material
  int wPawns = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[0]));
  int wKnights = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[1]));
  int wBishops = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[2]));
  int wRooks = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[3]));
  int wQueens = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[4]));

  int bPawns = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[6]));
  int bKnights = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[7]));
  int bBishops = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[8]));
  int bRooks = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[9]));
  int bQueens = __builtin_popcountll(static_cast<unsigned long long>(board.pieceBitboards[10]));

  int wMinor = wKnights + wBishops;
  int bMinor = bKnights + bBishops;
  int wMajor = wRooks + wQueens;
  int bMajor = bRooks + bQueens;
  int wTotal = wMinor + wMajor;
  int bTotal = bMinor + bMajor;

  int wKingSq = Chess::findKing(board, true);
  int bKingSq = Chess::findKing(board, false);
  if (wKingSq == -1 || bKingSq == -1) return score;

  int wKingRank = Chess::rowOf(wKingSq);
  int wKingFile = Chess::colOf(wKingSq);
  int bKingRank = Chess::rowOf(bKingSq);
  int bKingFile = Chess::colOf(bKingSq);

  // Distance of king to center (0=center, higher=edge)
  auto distToCenter = [](int rank, int file) -> int {
    int rd = std::max(3 - rank, rank - 4);
    int fd = std::max(3 - file, file - 4);
    return rd + fd;
  };

  // Distance between the two kings
  int kingDist = std::max(std::abs(wKingRank - bKingRank),
                          std::abs(wKingFile - bKingFile));

  // --- Pattern 1: Mating with overwhelming material ---
  // When one side has mating material and the other doesn't, push the losing
  // king to the corner/edge and bring the winning king closer
  bool whiteWinning = (score > 200);
  bool blackWinning = (score < -200);

  if (whiteWinning && bPawns == 0 && bTotal == 0) {
    // White has material, black has only king
    // Reward: black king near corner + kings close together
    score += distToCenter(bKingRank, bKingFile) * 15;
    score += (14 - kingDist) * 8;
    // Edge bonus
    if (bKingRank == 0 || bKingRank == 7 || bKingFile == 0 || bKingFile == 7)
      score += 30;
  }
  if (blackWinning && wPawns == 0 && wTotal == 0) {
    // Black has material, white has only king
    score -= distToCenter(wKingRank, wKingFile) * 15;
    score -= (14 - kingDist) * 8;
    if (wKingRank == 0 || wKingRank == 7 || wKingFile == 0 || wKingFile == 7)
      score -= 30;
  }

  // --- Pattern 2: KPK - King and Pawn vs King ---
  // Key rule: passed pawn should advance, winning king should support it
  if (wTotal == 0 && bTotal == 0) {
    // Pure pawn endgame
    // King centralization is crucial
    score += distToCenter(bKingRank, bKingFile) * 3; // Push enemy king away
    score -= distToCenter(wKingRank, wKingFile) * 3; // Centralize our king

    // Bonus for king near passed pawns
    std::uint64_t wPawnBB = board.pieceBitboards[0];
    while (wPawnBB) {
      int sq = __builtin_ctzll(static_cast<unsigned long long>(wPawnBB));
      wPawnBB &= wPawnBB - 1;
      if (isPassedPawn(sq, true, board.pieceBitboards[6])) {
        int pawnRank = Chess::rowOf(sq);
        int pawnFile = Chess::colOf(sq);
        // Distance of friendly king to pawn
        int wKingDist = std::max(std::abs(wKingRank - pawnRank),
                                 std::abs(wKingFile - pawnFile));
        // Distance of enemy king to promotion square
        int bKingToPromo = std::max(std::abs(bKingRank - 7),
                                    std::abs(bKingFile - pawnFile));
        score += (7 - pawnRank) * 3;  // Advanced = better
        score -= wKingDist * 5;       // King should be near pawn
        score += bKingToPromo * 4;    // Enemy king far from promotion = good
      }
    }

    std::uint64_t bPawnBB = board.pieceBitboards[6];
    while (bPawnBB) {
      int sq = __builtin_ctzll(static_cast<unsigned long long>(bPawnBB));
      bPawnBB &= bPawnBB - 1;
      if (isPassedPawn(sq, false, board.pieceBitboards[0])) {
        int pawnRank = Chess::rowOf(sq);
        int pawnFile = Chess::colOf(sq);
        int bKingDist = std::max(std::abs(bKingRank - pawnRank),
                                 std::abs(bKingFile - pawnFile));
        int wKingToPromo = std::max(std::abs(wKingRank - 0),
                                    std::abs(wKingFile - pawnFile));
        score -= pawnRank * 3;
        score += bKingDist * 5;
        score -= wKingToPromo * 4;
      }
    }
  }

  // --- Pattern 3: Rook Endgames ---
  if (wQueens == 0 && bQueens == 0 && wMinor == 0 && bMinor == 0 &&
      (wRooks > 0 || bRooks > 0)) {
    // Rook on 7th rank bonus
    std::uint64_t wRookBB = board.pieceBitboards[3];
    while (wRookBB) {
      int sq = __builtin_ctzll(static_cast<unsigned long long>(wRookBB));
      wRookBB &= wRookBB - 1;
      if (Chess::rowOf(sq) == 6) score += 30; // Rook on 7th
      // Rook behind passed pawn
      int rookFile = Chess::colOf(sq);
      std::uint64_t filePawns = board.pieceBitboards[0] & FILE_MASKS[rookFile];
      while (filePawns) {
        int pSq = __builtin_ctzll(static_cast<unsigned long long>(filePawns));
        filePawns &= filePawns - 1;
        if (isPassedPawn(pSq, true, board.pieceBitboards[6]) &&
            Chess::rowOf(sq) < Chess::rowOf(pSq)) {
          score += 20; // Rook behind own passed pawn
        }
      }
    }

    std::uint64_t bRookBB = board.pieceBitboards[9];
    while (bRookBB) {
      int sq = __builtin_ctzll(static_cast<unsigned long long>(bRookBB));
      bRookBB &= bRookBB - 1;
      if (Chess::rowOf(sq) == 1) score -= 30; // Rook on 2nd
      int rookFile = Chess::colOf(sq);
      std::uint64_t filePawns = board.pieceBitboards[6] & FILE_MASKS[rookFile];
      while (filePawns) {
        int pSq = __builtin_ctzll(static_cast<unsigned long long>(filePawns));
        filePawns &= filePawns - 1;
        if (isPassedPawn(pSq, false, board.pieceBitboards[0]) &&
            Chess::rowOf(sq) > Chess::rowOf(pSq)) {
          score -= 20; // Rook behind own passed pawn
        }
      }
    }
  }

  // --- Pattern 4: Opposite-colored Bishop Endgames ---
  // These are very drawish, scale down the eval
  if (wBishops == 1 && bBishops == 1 && wKnights == 0 && bKnights == 0 &&
      wMajor == 0 && bMajor == 0) {
    // Find bishop square colors
    int wBishopColor = -1, bBishopColor = -1;
    for (int sq = 0; sq < 64; ++sq) {
      if (board.sqaures[sq] == 3)
        wBishopColor = (Chess::rowOf(sq) + Chess::colOf(sq)) % 2;
      else if (board.sqaures[sq] == -3)
        bBishopColor = (Chess::rowOf(sq) + Chess::colOf(sq)) % 2;
    }
    if (wBishopColor != -1 && bBishopColor != -1 &&
        wBishopColor != bBishopColor) {
      // Opposite colored bishops - scale eval toward draw
      score = score * 60 / 100;
    }
  }

  // --- Pattern 5: No pawns = likely draw if material is roughly equal ---
  if (wPawns == 0 && bPawns == 0) {
    // KN vs K, KB vs K = draw (cannot force mate)
    if (wTotal <= 1 && bTotal == 0) score = score / 8;
    if (bTotal <= 1 && wTotal == 0) score = score / 8;
    // KNN vs K = draw (cannot force mate with two knights)
    if (wKnights == 2 && wBishops == 0 && wMajor == 0 && bTotal == 0) score = score / 8;
    if (bKnights == 2 && bBishops == 0 && bMajor == 0 && wTotal == 0) score = score / 8;
  }

  // --- General endgame: King centralization ---
  if (phase <= 6) {
    score -= distToCenter(wKingRank, wKingFile) * 5;
    score += distToCenter(bKingRank, bKingFile) * 5;
  }

  return score;
}

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

  // === Endgame Knowledge ===
  score = evaluateEndgame(board, score, phase);

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
