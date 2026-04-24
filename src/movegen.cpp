#include "movegen.h"
#include "utils.h"
#include "bitboard.h"
#include <cmath>

namespace Chess {

int findKing(const Board &board, bool white) {
  const std::uint64_t kingBB = board.pieceBitboards[white ? 5 : 11];
  if (kingBB == 0) return -1;
  return Bitboards::lsb(kingBB);
}

bool isPathClear(const Board &board, int from, int to) {
  const int fromRow = rowOf(from);
  const int fromCol = colOf(from);
  const int toRow = rowOf(to);
  const int toCol = colOf(to);

  const int rowStep = (toRow > fromRow) - (toRow < fromRow);
  const int colStep = (toCol > fromCol) - (toCol < fromCol);

  int currentRow = fromRow + rowStep;
  int currentCol = fromCol + colStep;

  while (currentRow != toRow || currentCol != toCol) {
    if (board.sqaures[currentRow * 8 + currentCol] != 0)
      return false;
    currentRow += rowStep;
    currentCol += colStep;
  }

  return true;
}

bool isSquareAttacked(const Board &board, int square, bool byWhite) {
  // Pawn attacks
  const int pawnColor = byWhite ? 1 : 0; // Reverse: white attacks come from white pawns
  const std::uint64_t pawns = board.pieceBitboards[byWhite ? 0 : 6];
  // Use reverse lookup: if a pawn of the attacking color can attack this square
  if (Bitboards::PAWN_ATTACKS[byWhite ? 1 : 0][square] & pawns)
    return true;

  // Knight attacks
  const std::uint64_t knights = board.pieceBitboards[byWhite ? 1 : 7];
  if (Bitboards::KNIGHT_ATTACKS[square] & knights)
    return true;

  // King attacks
  const std::uint64_t kings = board.pieceBitboards[byWhite ? 5 : 11];
  if (Bitboards::KING_ATTACKS[square] & kings)
    return true;

  // Bishop/Queen attacks (diagonal)
  const std::uint64_t bishopsQueens =
      board.pieceBitboards[byWhite ? 2 : 8] | board.pieceBitboards[byWhite ? 4 : 10];
  if (Bitboards::bishopAttacks(square, board.allPieces) & bishopsQueens)
    return true;

  // Rook/Queen attacks (orthogonal)
  const std::uint64_t rooksQueens =
      board.pieceBitboards[byWhite ? 3 : 9] | board.pieceBitboards[byWhite ? 4 : 10];
  if (Bitboards::rookAttacks(square, board.allPieces) & rooksQueens)
    return true;

  return false;
}

bool isInCheck(const Board &board, bool white) {
  const int kingSquare = findKing(board, white);
  if (kingSquare == -1)
    return true;
  return isSquareAttacked(board, kingSquare, !white);
}
void clearCastlingRightForRookSquare(Board &board, int square) {
  if (square == 7)
    board.castle &= ~Board::WHITE_KINGSIDE;
  if (square == 0)
    board.castle &= ~Board::WHITE_QUEENSIDE;
  if (square == 63)
    board.castle &= ~Board::BLACK_KINGSIDE;
  if (square == 56)
    board.castle &= ~Board::BLACK_QUEENSIDE;
}

bool makeMove(Board &board, int from, int to, int promotionPiece);

bool makeMove(Board &board, int from, int to) {
  return makeMove(board, from, to, 5);
}

bool makeMove(Board &board, int from, int to, int promotionPiece) {
  if (!isInside(from) || !isInside(to) || from == to)
    return false;

  const int movingPiece = board.sqaures[from];
  if (movingPiece == 0)
    return false;

  const bool movingWhite = movingPiece > 0;
  if (movingWhite != board.whiteTurn)
    return false;

  const int destinationPiece = board.sqaures[to];
  if (destinationPiece != 0 && (destinationPiece > 0) == movingWhite)
    return false;

  const int fromRow = rowOf(from);
  const int fromCol = colOf(from);
  const int toRow = rowOf(to);
  const int toCol = colOf(to);
  const int rowDiff = toRow - fromRow;
  const int colDiff = toCol - fromCol;
  const int absRowDiff = std::abs(rowDiff);
  const int absColDiff = std::abs(colDiff);
  const int piece = std::abs(movingPiece);

  bool isCapture = destinationPiece != 0;
  bool enPassantCapture = false;
  int enPassantCapturedSquare = -1;
  bool castlingMove = false;
  int rookFrom = -1;
  int rookTo = -1;
  int newEnPassantSquare = -1;

  switch (piece) {
  case 1: {
    const int forward = movingWhite ? 1 : -1;
    const int startRow = movingWhite ? 1 : 6;

    const bool oneStep =
        colDiff == 0 && rowDiff == forward && destinationPiece == 0;
    const bool twoStep = colDiff == 0 && rowDiff == 2 * forward &&
                         fromRow == startRow && destinationPiece == 0 &&
                         board.sqaures[from + 8 * forward] == 0;
    const bool diagonalCapture = absColDiff == 1 && rowDiff == forward &&
                                 destinationPiece != 0 &&
                                 (destinationPiece > 0) != movingWhite;
    const bool enPassant = absColDiff == 1 && rowDiff == forward &&
                           destinationPiece == 0 && to == board.enPassantSquare;

    if (!oneStep && !twoStep && !diagonalCapture && !enPassant)
      return false;

    if (enPassant) {
      enPassantCapture = true;
      enPassantCapturedSquare = to - 8 * forward;
      if (!isInside(enPassantCapturedSquare))
        return false;
      const int capturedPawn = board.sqaures[enPassantCapturedSquare];
      if (std::abs(capturedPawn) != 1 || (capturedPawn > 0) == movingWhite)
        return false;
      isCapture = true;
    }

    if (twoStep)
      newEnPassantSquare = from + 8 * forward;
    break;
  }
  case 2:
    if (!((absRowDiff == 2 && absColDiff == 1) ||
          (absRowDiff == 1 && absColDiff == 2)))
      return false;
    break;
  case 3:
    if (absRowDiff != absColDiff || !isPathClear(board, from, to))
      return false;
    break;
  case 4:
    if ((fromRow != toRow && fromCol != toCol) || !isPathClear(board, from, to))
      return false;
    break;
  case 5:
    if (!(fromRow == toRow || fromCol == toCol || absRowDiff == absColDiff) ||
        !isPathClear(board, from, to))
      return false;
    break;
  case 6: {
    if (absRowDiff <= 1 && absColDiff <= 1) {
      // Normal king move
    } else if (rowDiff == 0 && absColDiff == 2) {
      const int kingStart = movingWhite ? 4 : 60;
      if (from != kingStart)
        return false;
      if (isInCheck(board, movingWhite))
        return false;

      const bool kingSide = colDiff == 2;
      const int neededRight =
          movingWhite
              ? (kingSide ? Board::WHITE_KINGSIDE : Board::WHITE_QUEENSIDE)
              : (kingSide ? Board::BLACK_KINGSIDE : Board::BLACK_QUEENSIDE);
      if ((board.castle & neededRight) == 0)
        return false;

      rookFrom = movingWhite ? (kingSide ? 7 : 0) : (kingSide ? 63 : 56);
      rookTo = movingWhite ? (kingSide ? 5 : 3) : (kingSide ? 61 : 59);
      const int rookExpected = movingWhite ? 4 : -4;
      if (board.sqaures[rookFrom] != rookExpected)
        return false;

      const int rookCol = colOf(rookFrom);
      const int step = kingSide ? 1 : -1;
      for (int c = fromCol + step; c != rookCol; c += step) {
        const int square = fromRow * 8 + c;
        if (board.sqaures[square] != 0)
          return false;
      }

      if (isSquareAttacked(board, from + step, !movingWhite) ||
          isSquareAttacked(board, to, !movingWhite))
        return false;
      castlingMove = true;
    } else {
      return false;
    }
    break;
  }
  default:
    return false;
  }

  Board previous = board;

  // Apply the move using incremental bitboard updates
  if (isCapture && !enPassantCapture) {
    // Remove captured piece first, then move
    board.removePiece(to);
  }
  board.movePiece(from, to);

  if (enPassantCapture) {
    board.removePiece(enPassantCapturedSquare);
  }

  if (castlingMove) {
    board.movePiece(rookFrom, rookTo);
  }

  if (piece == 1 && (toRow == 0 || toRow == 7)) {
    int promotionType = promotionPiece;
    if (promotionType < 2 || promotionType > 5)
      promotionType = 5;
    // Remove the pawn that just moved, put the promoted piece
    board.removePiece(to);
    board.putPiece(movingWhite ? promotionType : -promotionType, to);
  }

  if (piece == 6) {
    if (movingWhite) {
      board.castle &= ~(Board::WHITE_KINGSIDE | Board::WHITE_QUEENSIDE);
    } else {
      board.castle &= ~(Board::BLACK_KINGSIDE | Board::BLACK_QUEENSIDE);
    }
  }

  if (piece == 4)
    clearCastlingRightForRookSquare(board, from);
  if (std::abs(destinationPiece) == 4)
    clearCastlingRightForRookSquare(board, to);

  board.enPassantSquare = newEnPassantSquare;

  if (piece == 1 || isCapture) {
    board.halfClock = 0;
  } else {
    ++board.halfClock;
  }

  board.whiteTurn = !board.whiteTurn;
  if (!movingWhite)
    ++board.moveNumber;

  if (isInCheck(board, movingWhite)) {
    board = previous;
    return false;
  }

  // Bitboards are already up-to-date via incremental updates — no rebuildBitboards() needed
  board.zobristKey = positionHash(board);
  board.positionHistory.push_back(board.zobristKey);

  // Recompute NNUE accumulator for the new position
  if (NNUE::isReady()) {
    NNUE::computeAccumulator(board.nnueAccumulator, board.sqaures);
  }

  return true;
}
bool hasAnyLegalMove(const Board &board, bool white) {
  Board setup = board;
  setup.whiteTurn = white;
  return !generateLegalMoves(setup, false).empty();
}

bool isCheckmate(const Board &board, bool white) {
  return isInCheck(board, white) && !hasAnyLegalMove(board, white);
}

bool isStalemate(const Board &board, bool white) {
  return !isInCheck(board, white) && !hasAnyLegalMove(board, white);
}

bool isFiftyMoveDraw(const Board &board) { return board.halfClock >= 100; }

bool isThreefoldRepetitionDraw(const Board &board) {
  if (board.positionHistory.empty())
    return false;
  const std::uint64_t current = board.positionHistory.back();
  int count = 0;
  for (std::uint64_t hash : board.positionHistory) {
    if (hash == current)
      ++count;
  }
  return count >= 3;
}

bool isInsufficientMaterialDraw(const Board &board) {
  // If any pawns, rooks, or queens exist, not insufficient
  if (board.pieceBitboards[0] || board.pieceBitboards[6])  return false; // Pawns
  if (board.pieceBitboards[3] || board.pieceBitboards[9])  return false; // Rooks
  if (board.pieceBitboards[4] || board.pieceBitboards[10]) return false; // Queens

  int whiteKnights = Bitboards::popcount(board.pieceBitboards[1]);
  int whiteBishops = Bitboards::popcount(board.pieceBitboards[2]);
  int blackKnights = Bitboards::popcount(board.pieceBitboards[7]);
  int blackBishops = Bitboards::popcount(board.pieceBitboards[8]);

  int whiteMinor = whiteKnights + whiteBishops;
  int blackMinor = blackKnights + blackBishops;

  // K vs K
  if (whiteMinor == 0 && blackMinor == 0) return true;
  // KN vs K or KB vs K
  if (whiteMinor == 1 && blackMinor == 0) return true;
  if (whiteMinor == 0 && blackMinor == 1) return true;
  // KNN vs K
  if (whiteKnights == 2 && whiteBishops == 0 && blackMinor == 0) return true;
  if (blackKnights == 2 && blackBishops == 0 && whiteMinor == 0) return true;

  // KB vs KB (same color bishops)
  if (whiteKnights == 0 && blackKnights == 0 &&
      whiteBishops == 1 && blackBishops == 1) {
    // Find bishop square colors via bitboard
    int wbSq = Bitboards::lsb(board.pieceBitboards[2]);
    int bbSq = Bitboards::lsb(board.pieceBitboards[8]);
    int wbColor = (Chess::rowOf(wbSq) + Chess::colOf(wbSq)) % 2;
    int bbColor = (Chess::rowOf(bbSq) + Chess::colOf(bbSq)) % 2;
    if (wbColor == bbColor) return true;
  }

  return false;
}

bool isDraw(const Board &board) {
  return isStalemate(board, board.whiteTurn) || isFiftyMoveDraw(board) ||
         isThreefoldRepetitionDraw(board) || isInsufficientMaterialDraw(board);
}
int countLegalMoves(const Board &board, bool white) {
  Board setup = board;
  setup.whiteTurn = white;
  return static_cast<int>(generateLegalMoves(setup, false).size());
}
int popLeastSignificantBit(std::uint64_t &bitboard) {
  const int square = __builtin_ctzll(static_cast<unsigned long long>(bitboard));
  bitboard &= bitboard - 1;
  return square;
}
bool applyMove(Board &board, const Move &move) {
  if (move.promotion == 0)
    return Chess::makeMove(board, move.from, move.to);
  return Chess::makeMove(board, move.from, move.to, move.promotion);
}
std::vector<Move> generateLegalMoves(const Board &board, bool capturesOnly) {
  std::vector<Move> moves;
  const bool whiteToMove = board.whiteTurn;
  const std::uint64_t friendlyOcc =
      whiteToMove ? board.whitePieces : board.blackPieces;
  const std::uint64_t enemyOcc =
      whiteToMove ? board.blackPieces : board.whitePieces;

  auto tryAddMove = [&](int from, int to, int pieceType) {
    if (!Chess::isInside(to))
      return;

    const std::uint64_t toBit = 1ULL << to;
    if (friendlyOcc & toBit)
      return;

    int capturedType = std::abs(board.sqaures[to]);
    if (capturedType == 0 && pieceType == 1 && to == board.enPassantSquare &&
        Chess::colOf(from) != Chess::colOf(to)) {
      const int capturedSquare = to + (whiteToMove ? -8 : 8);
      if (Chess::isInside(capturedSquare)) {
        capturedType = std::abs(board.sqaures[capturedSquare]);
      }
    }

    const bool isCapture = capturedType != 0;
    if (capturesOnly && !isCapture)
      return;

    const bool promotionSquare =
        pieceType == 1 && ((whiteToMove && Chess::rowOf(to) == 7) ||
                           (!whiteToMove && Chess::rowOf(to) == 0));

    const int firstPromotion = promotionSquare ? 2 : 0;
    const int lastPromotion = promotionSquare ? 5 : 0;

    for (int promotion = firstPromotion; promotion <= lastPromotion;
         ++promotion) {
      Move move;
      move.from = from;
      move.to = to;
      move.promotion = promotion;
      move.movingType = pieceType;
      move.capturedType = capturedType;
      move.isCapture = isCapture;

      Board next = board;
      const bool legal = (promotion == 0)
                             ? Chess::makeMove(next, from, to)
                             : Chess::makeMove(next, from, to, promotion);

      if (legal)
        moves.push_back(move);
    }
  };

  static const int knightOffsets[8][2] = {{2, 1}, {2, -1}, {-2, 1}, {-2, -1},
                                          {1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
  static const int kingOffsets[8][2] = {{1, 0},  {1, 1},   {0, 1},  {-1, 1},
                                        {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
  static const int bishopDirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  static const int rookDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

  std::uint64_t pieces = friendlyOcc;
  while (pieces) {
    const int from = popLeastSignificantBit(pieces);
    const int piece = board.sqaures[from];
    const int pieceType = std::abs(piece);
    const int row = Chess::rowOf(from);
    const int col = Chess::colOf(from);

    switch (pieceType) {
    case 1: {
      const int forward = whiteToMove ? 8 : -8;
      const int oneStep = from + forward;

      if (!capturesOnly && Chess::isInside(oneStep) &&
          board.sqaures[oneStep] == 0) {
        tryAddMove(from, oneStep, pieceType);

        const int startRow = whiteToMove ? 1 : 6;
        const int twoStep = from + 2 * forward;
        if (row == startRow && Chess::isInside(twoStep) &&
            board.sqaures[twoStep] == 0) {
          tryAddMove(from, twoStep, pieceType);
        }
      }

      // Pawn captures using attack tables
      std::uint64_t pawnAtk = Bitboards::PAWN_ATTACKS[whiteToMove ? 0 : 1][from];
      std::uint64_t pawnCaptures = pawnAtk & enemyOcc;
      // Add en passant square
      if (board.enPassantSquare >= 0 &&
          (pawnAtk & (1ULL << board.enPassantSquare))) {
        pawnCaptures |= 1ULL << board.enPassantSquare;
      }
      while (pawnCaptures) {
        int to = Bitboards::popLSB(pawnCaptures);
        tryAddMove(from, to, pieceType);
      }
      break;
    }
    case 2: {
      // Knight moves via lookup table
      std::uint64_t targets = Bitboards::KNIGHT_ATTACKS[from] & ~friendlyOcc;
      if (capturesOnly) targets &= enemyOcc;
      while (targets) {
        int to = Bitboards::popLSB(targets);
        tryAddMove(from, to, pieceType);
      }
      break;
    }
    case 3: {
      // Bishop moves via attack generation
      std::uint64_t targets = Bitboards::bishopAttacks(from, board.allPieces) & ~friendlyOcc;
      if (capturesOnly) targets &= enemyOcc;
      while (targets) {
        int to = Bitboards::popLSB(targets);
        tryAddMove(from, to, pieceType);
      }
      break;
    }
    case 4: {
      // Rook moves via attack generation
      std::uint64_t targets = Bitboards::rookAttacks(from, board.allPieces) & ~friendlyOcc;
      if (capturesOnly) targets &= enemyOcc;
      while (targets) {
        int to = Bitboards::popLSB(targets);
        tryAddMove(from, to, pieceType);
      }
      break;
    }
    case 5: {
      // Queen moves via attack generation
      std::uint64_t targets = Bitboards::queenAttacks(from, board.allPieces) & ~friendlyOcc;
      if (capturesOnly) targets &= enemyOcc;
      while (targets) {
        int to = Bitboards::popLSB(targets);
        tryAddMove(from, to, pieceType);
      }
      break;
    }
    case 6: {
      // King moves via lookup table
      std::uint64_t targets = Bitboards::KING_ATTACKS[from] & ~friendlyOcc;
      if (capturesOnly) targets &= enemyOcc;
      while (targets) {
        int to = Bitboards::popLSB(targets);
        tryAddMove(from, to, pieceType);
      }

      // Castling
      if (!capturesOnly) {
        tryAddMove(from, from + 2, pieceType);
        tryAddMove(from, from - 2, pieceType);
      }
      break;
    }
    default:
      break;
    }
  }

  return moves;
}

} // namespace Chess
