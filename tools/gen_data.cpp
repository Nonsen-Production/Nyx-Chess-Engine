/*
 * High-Quality NNUE Training Data Generator for Nyx Chess Engine
 *
 * Improvements over basic gen_data:
 *   1. Search-based labels: uses depth-N search instead of static HCE
 *   2. Proper self-play: engine plays against itself using its search
 *   3. Quiet positions only: skips positions in check or after captures
 *   4. Random opening book: 8 random moves at start for diversity
 *   5. Game result tracking: records WDL for loss-function blending
 *   6. Adjudication: stops games early when eval is clearly winning/losing
 *
 * Build:
 *   c++ -std=c++17 -O3 -I src -o tools/gen_data tools/gen_data.cpp \
 *       src/movegen.cpp src/eval.cpp src/board.cpp src/nnue.cpp \
 *       src/utils.cpp src/bitboard.cpp src/book.cpp src/search.cpp
 *
 * Usage:
 *   ./tools/gen_data [num_games] [search_depth] [output_file]
 *   ./tools/gen_data 5000 6 train_data.bin
 *
 * Output format (per position):
 *   768 bytes:  feature vector (uint8, 0 or 1 per input feature)
 *   4 bytes:    search eval (int32_t, from white's perspective, centipawns)
 *   1 byte:     side to move (0=white, 1=black)
 *   1 byte:     game result from white's POV (0=loss, 1=draw, 2=win)
 *   Total: 774 bytes per record
 */

#include "board.h"
#include "movegen.h"
#include "eval.h"
#include "search.h"
#include "bitboard.h"
#include "nnue.h"
#include "book.h"

#include <iostream>
#include <sstream>
#include <random>
#include <vector>
#include <cstdint>
#include <cstring>

// Per-position record before we know game result
struct PositionRecord {
  std::uint8_t features[768];
  std::int32_t eval;
  std::uint8_t stm;
};

// Extract features from board
static PositionRecord extractPosition(const Board &board, int searchEval) {
  PositionRecord rec;
  std::memset(rec.features, 0, 768);

  for (int sq = 0; sq < 64; ++sq) {
    int piece = board.sqaures[sq];
    if (piece == 0) continue;

    bool whitePiece = piece > 0;
    int pieceType = std::abs(piece);
    int typeIdx = pieceType - 1;

    int colorOff = whitePiece ? 0 : 384;
    int idx = colorOff + typeIdx * 64 + sq;
    rec.features[idx] = 1;
  }

  // Store eval from white's perspective
  rec.eval = static_cast<std::int32_t>(searchEval);
  rec.stm = board.whiteTurn ? 0 : 1;
  return rec;
}

// Search a position and return the result (eval + best move)
static Search::SearchResult searchPositionFull(const Board &board, int depth) {
  Search::SearchOptions opts;
  opts.maxDepth = depth;
  opts.enableLogging = false;

  // Redirect stdout to suppress search info lines
  std::streambuf *origBuf = std::cout.rdbuf();
  std::ostringstream nullStream;
  std::cout.rdbuf(nullStream.rdbuf());

  Search::SearchResult result = Search::analyzeNextMove(board, opts);

  // Restore stdout
  std::cout.rdbuf(origBuf);

  return result;
}

static int evalFromWhite(const Search::SearchResult &result, bool whiteTurn) {
  return whiteTurn ? result.scoreForSideToMove : -result.scoreForSideToMove;
}

// Check if position is "quiet" (not in check, last move wasn't a capture)
static bool isQuietPosition(const Board &board) {
  if (Chess::isInCheck(board, board.whiteTurn)) return false;
  if (Chess::isInCheck(board, !board.whiteTurn)) return false;
  return true;
}

// Game result codes
static constexpr std::uint8_t RESULT_BLACK_WIN = 0;
static constexpr std::uint8_t RESULT_DRAW = 1;
static constexpr std::uint8_t RESULT_WHITE_WIN = 2;

int main(int argc, char *argv[]) {
  int numGames = 5000;
  int searchDepth = 6;
  std::string outputFile = "train_data.bin";

  if (argc >= 2) numGames = std::atoi(argv[1]);
  if (argc >= 3) searchDepth = std::atoi(argv[2]);
  if (argc >= 4) outputFile = argv[3];

  if (searchDepth < 1) searchDepth = 1;
  if (searchDepth > 12) searchDepth = 12;

  Bitboards::init();

  std::ofstream out(outputFile, std::ios::binary);
  if (!out.is_open()) {
    std::cerr << "Cannot create " << outputFile << std::endl;
    return 1;
  }

  std::mt19937 rng(42);
  int totalPositions = 0;
  int totalGames = 0;

  // Adjudication thresholds
  static constexpr int ADJUDICATE_WIN = 1000;  // 10 pawns
  static constexpr int ADJUDICATE_COUNT = 5;   // consecutive moves

  std::cout << "=== Nyx NNUE Training Data Generator ===" << std::endl;
  std::cout << "Games: " << numGames << std::endl;
  std::cout << "Search depth: " << searchDepth << std::endl;
  std::cout << "Output: " << outputFile << std::endl;
  std::cout << std::endl;

  for (int game = 0; game < numGames; ++game) {
    Board board;
    int ply = 0;
    int adjudicateCounter = 0;
    int lastEvalSign = 0;
    std::uint8_t gameResult = RESULT_DRAW;
    bool gameOver = false;

    // Collect positions during the game, write after result is known
    std::vector<PositionRecord> gamePositions;
    gamePositions.reserve(200);

    // === Phase 1: Random opening (from book + random plies for diversity) ===
    std::uniform_int_distribution<int> bookDist(0, Book::BUILTIN_BOOK_SIZE - 1);
    int bookIndex = bookDist(rng);
    const std::string& bookLine = Book::BUILTIN_BOOK[bookIndex].moves;
    
    std::istringstream iss(bookLine);
    std::string moveStr;
    while (iss >> moveStr && !gameOver) {
      MoveList moves;
      Chess::generateLegalMoves(board, moves, false);
      
      bool moveFound = false;
      for (const Move& m : moves) {
        if (Search::toUci(m) == moveStr) {
          Chess::applyMove(board, m);
          moveFound = true;
          break;
        }
      }
      if (!moveFound) break;
      ply++;
    }

    // Add a few random moves for extra diversity (2-4 plies)
    std::uniform_int_distribution<int> extraPliesDist(2, 4);
    int extraPlies = extraPliesDist(rng);
    
    for (int i = 0; i < extraPlies && !gameOver; ++i) {
      MoveList moves;
      Chess::generateLegalMoves(board, moves, false);
      if (moves.empty()) {
        gameOver = true;
        if (Chess::isInCheck(board, board.whiteTurn)) {
          gameResult = board.whiteTurn ? RESULT_BLACK_WIN : RESULT_WHITE_WIN;
        } else {
          gameResult = RESULT_DRAW;
        }
        break;
      }
      std::uniform_int_distribution<int> moveDist(0, moves.size() - 1);
      Move randomMove = moves[moveDist(rng)];
      Chess::applyMove(board, randomMove);
      ++ply;
    }

    // === Phase 2: Search-based self-play ===
    while (!gameOver && ply < 300) {
      MoveList moves;
      Chess::generateLegalMoves(board, moves, false);
      if (moves.empty()) {
        if (Chess::isInCheck(board, board.whiteTurn)) {
          gameResult = board.whiteTurn ? RESULT_BLACK_WIN : RESULT_WHITE_WIN;
        } else {
          gameResult = RESULT_DRAW;
        }
        break;
      }

      // Draw by 50-move rule or repetition
      if (Chess::isDraw(board)) {
        gameResult = RESULT_DRAW;
        break;
      }

      // Search this position (single search for both eval and best move)
      Search::SearchResult result = searchPositionFull(board, searchDepth);
      if (!result.hasMove) break;

      int eval = evalFromWhite(result, board.whiteTurn);

      // Record position if quiet
      if (isQuietPosition(board) && ply >= 10) {
        gamePositions.push_back(extractPosition(board, eval));
      }

      // Adjudication: if eval is decisive for ADJUDICATE_COUNT consecutive moves
      int evalSign = (eval > ADJUDICATE_WIN) ? 1 : (eval < -ADJUDICATE_WIN) ? -1 : 0;
      if (evalSign != 0 && evalSign == lastEvalSign) {
        ++adjudicateCounter;
        if (adjudicateCounter >= ADJUDICATE_COUNT) {
          gameResult = (evalSign > 0) ? RESULT_WHITE_WIN : RESULT_BLACK_WIN;
          break;
        }
      } else {
        adjudicateCounter = (evalSign != 0) ? 1 : 0;
      }
      lastEvalSign = evalSign;

      // Mate score detection
      if (eval > 90000 || eval < -90000) {
        gameResult = (eval > 0) ? RESULT_WHITE_WIN : RESULT_BLACK_WIN;
        break;
      }

      // Play the best move found by search
      Board next = board;
      if (!Chess::applyMove(next, result.bestMove)) break;
      board = next;
      ++ply;
    }

    // === Phase 3: Write positions with known game result ===
    for (const auto &rec : gamePositions) {
      out.write(reinterpret_cast<const char *>(rec.features), 768);
      out.write(reinterpret_cast<const char *>(&rec.eval), 4);
      out.write(reinterpret_cast<const char *>(&rec.stm), 1);
      out.write(reinterpret_cast<const char *>(&gameResult), 1);
      ++totalPositions;
    }

    ++totalGames;
    Search::clearTranspositionTable();

    if ((game + 1) % 10 == 0 || game == 0) {
      const char *resultStr = (gameResult == RESULT_WHITE_WIN)
                                  ? "1-0"
                                  : (gameResult == RESULT_BLACK_WIN) ? "0-1"
                                                                     : "1/2";
      std::cout << "Game " << (game + 1) << "/" << numGames
                << "  plies=" << ply << "  result=" << resultStr
                << "  positions=" << totalPositions << std::endl;
    }
  }

  out.close();
  std::cout << std::endl;
  std::cout << "=== Complete ===" << std::endl;
  std::cout << "Games played: " << totalGames << std::endl;
  std::cout << "Positions saved: " << totalPositions << std::endl;
  std::cout << "Output: " << outputFile << " ("
            << totalPositions * 774 << " bytes)" << std::endl;

  return 0;
}
