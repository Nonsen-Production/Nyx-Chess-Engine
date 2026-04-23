#include "board.h"
#include "utils.h"
#include <iostream>
#include <sstream>

namespace Chess {

void rebuildBitboards(Board &board) {
  board.pieceBitboards.fill(0);
  board.whitePieces = 0;
  board.blackPieces = 0;

  for (int square = 0; square < 64; ++square) {
    const int piece = board.sqaures[square];
    if (piece == 0)
      continue;

    const int index = pieceToIndex(piece);
    if (index < 0)
      continue;

    const std::uint64_t bit = 1ULL << square;
    board.pieceBitboards[index] |= bit;
    if (piece > 0) {
      board.whitePieces |= bit;
    } else {
      board.blackPieces |= bit;
    }
  }

  board.allPieces = board.whitePieces | board.blackPieces;
}

namespace Zobrist {

static bool initialized = false;
static std::uint64_t sideToMoveKey = 0;
static std::uint64_t pieceSquare[12][64] = {{0}};
static std::uint64_t castlingRights[16] = {0};
static std::uint64_t enPassantFile[8] = {0};

std::uint64_t splitmix64(std::uint64_t &state) {
  std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

void initialize() {
  if (initialized)
    return;

  std::uint64_t seed = 0xC0FFEE1234567890ULL;
  for (int piece = 0; piece < 12; ++piece) {
    for (int square = 0; square < 64; ++square) {
      pieceSquare[piece][square] = splitmix64(seed);
    }
  }

  for (int i = 0; i < 16; ++i)
    castlingRights[i] = splitmix64(seed);
  for (int i = 0; i < 8; ++i)
    enPassantFile[i] = splitmix64(seed);
  sideToMoveKey = splitmix64(seed);

  initialized = true;
}

std::uint64_t compute(const Board &board) {
  initialize();

  std::uint64_t key = 0;
  for (int square = 0; square < 64; ++square) {
    const int piece = board.sqaures[square];
    if (piece == 0)
      continue;

    const int pieceIndex = pieceToIndex(piece);
    if (pieceIndex >= 0) {
      key ^= pieceSquare[pieceIndex][square];
    }
  }

  key ^= castlingRights[board.castle & 0x0F];
  if (board.enPassantSquare != -1) {
    key ^= enPassantFile[colOf(board.enPassantSquare)];
  }
  if (board.whiteTurn) {
    key ^= sideToMoveKey;
  }

  return key;
}

} // namespace Zobrist

std::uint64_t positionHash(const Board &board) {
  return Zobrist::compute(board);
}

bool loadFEN(Board &board, const std::string &fen) {
  std::istringstream parser(fen);
  std::string placement;
  std::string activeColor;
  std::string castlingRights;
  std::string enPassant;
  int halfmoveClock = 0;
  int fullmoveNumber = 1;

  if (!(parser >> placement >> activeColor >> castlingRights >> enPassant >>
        halfmoveClock >> fullmoveNumber)) {
    return false;
  }

  Board parsed;
  for (int &square : parsed.sqaures) {
    square = 0;
  }
  parsed.positionHistory.clear();
  parsed.castle = 0;
  parsed.enPassantSquare = -1;

  int rank = 7;
  int file = 0;
  for (char token : placement) {
    if (token == '/') {
      if (file != 8)
        return false;
      --rank;
      file = 0;
      continue;
    }

    if (token >= '1' && token <= '8') {
      file += token - '0';
      if (file > 8)
        return false;
      continue;
    }

    const int piece = pieceFromFenChar(token);
    if (piece == 0 || rank < 0 || file >= 8)
      return false;
    parsed.sqaures[rank * 8 + file] = piece;
    ++file;
  }

  if (rank != 0 || file != 8)
    return false;

  if (activeColor == "w") {
    parsed.whiteTurn = true;
  } else if (activeColor == "b") {
    parsed.whiteTurn = false;
  } else {
    return false;
  }

  if (castlingRights != "-") {
    for (char right : castlingRights) {
      switch (right) {
      case 'K':
        parsed.castle |= Board::WHITE_KINGSIDE;
        break;
      case 'Q':
        parsed.castle |= Board::WHITE_QUEENSIDE;
        break;
      case 'k':
        parsed.castle |= Board::BLACK_KINGSIDE;
        break;
      case 'q':
        parsed.castle |= Board::BLACK_QUEENSIDE;
        break;
      default:
        return false;
      }
    }
  }

  if (enPassant != "-") {
    const int enPassantSquare = squareFromAlgebraic(enPassant);
    if (!isInside(enPassantSquare))
      return false;
    parsed.enPassantSquare = enPassantSquare;
  }

  parsed.halfClock = halfmoveClock;
  parsed.moveNumber = fullmoveNumber;

  int whiteKings = 0;
  int blackKings = 0;
  for (int square : parsed.sqaures) {
    if (square == 6)
      ++whiteKings;
    if (square == -6)
      ++blackKings;
  }
  if (whiteKings != 1 || blackKings != 1)
    return false;

  rebuildBitboards(parsed);
  parsed.zobristKey = positionHash(parsed);
  parsed.positionHistory.push_back(parsed.zobristKey);
  board = parsed;
  return true;
}

bool loadPositionFromParts(Board &board, const std::string &boardPlacement,
                           const std::string &castlingRights,
                           const std::string &sideToMove) {
  if (boardPlacement.empty() || sideToMove.empty())
    return false;

  const char side = std::tolower(static_cast<unsigned char>(sideToMove[0]));
  if (side != 'w' && side != 'b')
    return false;

  const std::string castling = castlingRights.empty() ? "-" : castlingRights;
  const std::string fen =
      boardPlacement + " " + std::string(1, side) + " " + castling + " - 0 1";
  return loadFEN(board, fen);
}

} // namespace Chess

Board::Board() {
  int startSet[8] = {4, 2, 3, 5, 6, 3, 2, 4};
  for (int i = 0; i < 8; ++i) {
    sqaures[i] = startSet[i];
    sqaures[i + 8] = 1;
    sqaures[63 - i - 8] = -1;
    sqaures[63 - i] = -startSet[i];
  }

  whiteTurn = true;
  castle = WHITE_KINGSIDE | WHITE_QUEENSIDE | BLACK_KINGSIDE | BLACK_QUEENSIDE;
  moveNumber = 1;
  halfClock = 0;
  enPassantSquare = -1;
  Chess::rebuildBitboards(*this);
  zobristKey = Chess::positionHash(*this);
  positionHistory.push_back(zobristKey);

  // Initialize NNUE accumulator if network is loaded
  if (NNUE::isReady()) {
    NNUE::computeAccumulator(nnueAccumulator, sqaures);
  }
}

void Board::printBoard() const {
  for (int r = 7; r >= 0; --r) {
    for (int c = 0; c < 8; ++c) {
      std::cout << sqaures[r * 8 + c] << '\t';
    }
    std::cout << '\n';
  }
}
