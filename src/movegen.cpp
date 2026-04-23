#include "movegen.h"
#include "utils.h"
#include <cmath>

namespace Chess {

int findKing(const Board &board, bool white) {
  const int king = white ? 6 : -6;
  for (int i = 0; i < 64; ++i) {
    if (board.sqaures[i] == king)
      return i;
  }
  return -1;
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
  const int targetRow = rowOf(square);
  const int targetCol = colOf(square);

  const int pawn = byWhite ? 1 : -1;
  const int pawnRow = byWhite ? targetRow - 1 : targetRow + 1;
  if (pawnRow >= 0 && pawnRow < 8) {
    if (targetCol > 0 && board.sqaures[pawnRow * 8 + targetCol - 1] == pawn)
      return true;
    if (targetCol < 7 && board.sqaures[pawnRow * 8 + targetCol + 1] == pawn)
      return true;
  }

  const int knight = byWhite ? 2 : -2;
  static const int knightOffsets[8][2] = {{2, 1}, {2, -1}, {-2, 1}, {-2, -1},
                                          {1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
  for (const auto &offset : knightOffsets) {
    const int r = targetRow + offset[0];
    const int c = targetCol + offset[1];
    if (r >= 0 && r < 8 && c >= 0 && c < 8 &&
        board.sqaures[r * 8 + c] == knight)
      return true;
  }

  const int bishop = byWhite ? 3 : -3;
  const int rook = byWhite ? 4 : -4;
  const int queen = byWhite ? 5 : -5;
  const int king = byWhite ? 6 : -6;

  static const int diagonalDirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  for (const auto &dir : diagonalDirs) {
    int r = targetRow + dir[0];
    int c = targetCol + dir[1];
    while (r >= 0 && r < 8 && c >= 0 && c < 8) {
      const int piece = board.sqaures[r * 8 + c];
      if (piece != 0) {
        if (piece == bishop || piece == queen)
          return true;
        break;
      }
      r += dir[0];
      c += dir[1];
    }
  }

  static const int orthogonalDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (const auto &dir : orthogonalDirs) {
    int r = targetRow + dir[0];
    int c = targetCol + dir[1];
    while (r >= 0 && r < 8 && c >= 0 && c < 8) {
      const int piece = board.sqaures[r * 8 + c];
      if (piece != 0) {
        if (piece == rook || piece == queen)
          return true;
        break;
      }
      r += dir[0];
      c += dir[1];
    }
  }

  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0)
        continue;
      const int r = targetRow + dr;
      const int c = targetCol + dc;
      if (r >= 0 && r < 8 && c >= 0 && c < 8 &&
          board.sqaures[r * 8 + c] == king)
        return true;
    }
  }

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

  board.sqaures[to] = board.sqaures[from];
  board.sqaures[from] = 0;

  if (enPassantCapture) {
    board.sqaures[enPassantCapturedSquare] = 0;
  }

  if (castlingMove) {
    board.sqaures[rookTo] = board.sqaures[rookFrom];
    board.sqaures[rookFrom] = 0;
  }

  if (piece == 1 && (toRow == 0 || toRow == 7)) {
    int promotionType = promotionPiece;
    if (promotionType < 2 || promotionType > 5)
      promotionType = 5;
    board.sqaures[to] = movingWhite ? promotionType : -promotionType;
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

  rebuildBitboards(board);
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

  for (int from = 0; from < 64; ++from) {
    const int piece = setup.sqaures[from];
    if (piece == 0 || (piece > 0) != white)
      continue;

    for (int to = 0; to < 64; ++to) {
      Board attempt = setup;
      if (makeMove(attempt, from, to))
        return true;

      if (std::abs(piece) == 1 &&
          ((white && rowOf(to) == 7) || (!white && rowOf(to) == 0))) {
        for (int promotion = 2; promotion <= 4; ++promotion) {
          Board promotionAttempt = setup;
          if (makeMove(promotionAttempt, from, to, promotion))
            return true;
        }
      }
    }
  }

  return false;
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
  int whiteBishops = 0;
  int blackBishops = 0;
  int whiteKnights = 0;
  int blackKnights = 0;
  int whiteBishopColor = -1;
  int blackBishopColor = -1;

  for (int i = 0; i < 64; ++i) {
    const int piece = board.sqaures[i];
    if (piece == 0)
      continue;

    const int type = std::abs(piece);
    if (type == 1 || type == 4 || type == 5)
      return false;

    if (type == 3) {
      const int squareColor = (rowOf(i) + colOf(i)) % 2;
      if (piece > 0) {
        ++whiteBishops;
        whiteBishopColor = squareColor;
      } else {
        ++blackBishops;
        blackBishopColor = squareColor;
      }
    }

    if (type == 2) {
      if (piece > 0) {
        ++whiteKnights;
      } else {
        ++blackKnights;
      }
    }
  }

  const int whiteMinor = whiteBishops + whiteKnights;
  const int blackMinor = blackBishops + blackKnights;

  if (whiteMinor == 0 && blackMinor == 0)
    return true;
  if (whiteMinor == 1 && blackMinor == 0)
    return true;
  if (whiteMinor == 0 && blackMinor == 1)
    return true;
  if (whiteKnights == 2 && whiteBishops == 0 && blackMinor == 0)
    return true;
  if (blackKnights == 2 && blackBishops == 0 && whiteMinor == 0)
    return true;

  if (whiteKnights == 0 && blackKnights == 0 && whiteBishops == 1 &&
      blackBishops == 1 && whiteBishopColor == blackBishopColor) {
    return true;
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

  int legalMoves = 0;

  for (int from = 0; from < 64; ++from) {
    const int piece = setup.sqaures[from];
    if (piece == 0 || (piece > 0) != white)
      continue;

    for (int to = 0; to < 64; ++to) {
      if (std::abs(piece) == 1 && ((white && Chess::rowOf(to) == 7) ||
                                   (!white && Chess::rowOf(to) == 0))) {
        for (int promotion = 2; promotion <= 5; ++promotion) {
          Board attempt = setup;
          if (Chess::makeMove(attempt, from, to, promotion))
            ++legalMoves;
        }
      } else {
        Board attempt = setup;
        if (Chess::makeMove(attempt, from, to))
          ++legalMoves;
      }
    }
  }

  return legalMoves;
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

      if (col > 0) {
        const int leftCapture = from + (whiteToMove ? 7 : -9);
        if (Chess::isInside(leftCapture) &&
            ((enemyOcc & (1ULL << leftCapture)) ||
             leftCapture == board.enPassantSquare)) {
          tryAddMove(from, leftCapture, pieceType);
        }
      }
      if (col < 7) {
        const int rightCapture = from + (whiteToMove ? 9 : -7);
        if (Chess::isInside(rightCapture) &&
            ((enemyOcc & (1ULL << rightCapture)) ||
             rightCapture == board.enPassantSquare)) {
          tryAddMove(from, rightCapture, pieceType);
        }
      }
      break;
    }
    case 2: {
      for (const auto &offset : knightOffsets) {
        const int r = row + offset[0];
        const int c = col + offset[1];
        if (r < 0 || r > 7 || c < 0 || c > 7)
          continue;
        tryAddMove(from, r * 8 + c, pieceType);
      }
      break;
    }
    case 3:
    case 4:
    case 5: {
      const int (*dirs)[2] = (pieceType == 3) ? bishopDirs : rookDirs;
      const int dirCount = 4;

      for (int d = 0; d < dirCount; ++d) {
        int r = row + dirs[d][0];
        int c = col + dirs[d][1];

        while (r >= 0 && r < 8 && c >= 0 && c < 8) {
          const int to = r * 8 + c;
          const std::uint64_t bit = 1ULL << to;

          if (friendlyOcc & bit)
            break;

          if (enemyOcc & bit) {
            tryAddMove(from, to, pieceType);
            break;
          }

          if (!capturesOnly) {
            tryAddMove(from, to, pieceType);
          }

          r += dirs[d][0];
          c += dirs[d][1];
        }
      }

      if (pieceType == 5) {
        for (int d = 0; d < 4; ++d) {
          int r = row + bishopDirs[d][0];
          int c = col + bishopDirs[d][1];

          while (r >= 0 && r < 8 && c >= 0 && c < 8) {
            const int to = r * 8 + c;
            const std::uint64_t bit = 1ULL << to;

            if (friendlyOcc & bit)
              break;

            if (enemyOcc & bit) {
              tryAddMove(from, to, pieceType);
              break;
            }

            if (!capturesOnly) {
              tryAddMove(from, to, pieceType);
            }

            r += bishopDirs[d][0];
            c += bishopDirs[d][1];
          }
        }
      }
      break;
    }
    case 6: {
      for (const auto &offset : kingOffsets) {
        const int r = row + offset[0];
        const int c = col + offset[1];
        if (r < 0 || r > 7 || c < 0 || c > 7)
          continue;
        tryAddMove(from, r * 8 + c, pieceType);
      }

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
