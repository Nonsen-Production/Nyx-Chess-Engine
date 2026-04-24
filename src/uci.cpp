#include "uci.h"
#include "utils.h"
#include "movegen.h"
#include "search.h"
#include "book.h"
#include "nnue.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

extern std::string gGameMovesUCI;

namespace Chess {

bool applyUCIMove(Board &board, const std::string &uciMove) {
  if (uciMove.size() != 4 && uciMove.size() != 5)
    return false;

  const int from = squareFromAlgebraic(uciMove.substr(0, 2));
  const int to = squareFromAlgebraic(uciMove.substr(2, 2));
  if (!isInside(from) || !isInside(to))
    return false;

  int promotionPiece = 5;
  if (uciMove.size() == 5) {
    promotionPiece = promotionPieceFromUciChar(uciMove[4]);
    if (promotionPiece == 0)
      return false;
  }

  return makeMove(board, from, to, promotionPiece);
}
bool setPositionFromUCI(Board &board, const std::string &uciPositionCommand) {
  std::istringstream parser(uciPositionCommand);
  std::string token;

  if (!(parser >> token) || token != "position")
    return false;
  if (!(parser >> token))
    return false;

  Board parsed;
  if (token == "startpos") {
    parsed = Board();
  } else if (token == "fen") {
    std::string fen;
    std::string field;
    for (int i = 0; i < 6; ++i) {
      if (!(parser >> field))
        return false;
      if (!fen.empty())
        fen += ' ';
      fen += field;
    }
    if (!loadFEN(parsed, fen))
      return false;
  } else {
    return false;
  }

  if (!(parser >> token)) {
    board = parsed;
    return true;
  }

  if (token != "moves")
    return false;

  while (parser >> token) {
    if (!applyUCIMove(parsed, token))
      return false;
  }

  board = parsed;
  return true;
}

} // namespace Chess

// Helper: stop and join the search thread if running
static void stopSearchThread(std::thread &searchThread) {
  if (searchThread.joinable()) {
    Search::gStopSearch = true;
    searchThread.join();
  }
}

// Perft: count leaf nodes at a given depth to verify move generation
static std::uint64_t perft(Board &board, int depth) {
  if (depth == 0) return 1;

  std::vector<Move> moves = Chess::generateLegalMoves(board, false);
  std::uint64_t nodes = 0;

  for (const Move &move : moves) {
    Board next = board;
    if (!Chess::applyMove(next, move)) continue;
    nodes += perft(next, depth - 1);
  }

  return nodes;
}

// Divide perft: prints per-move node counts for debugging
static void dividePerft(Board &board, int depth) {
  auto startTime = std::chrono::steady_clock::now();

  std::vector<Move> moves = Chess::generateLegalMoves(board, false);
  std::uint64_t totalNodes = 0;

  for (const Move &move : moves) {
    Board next = board;
    if (!Chess::applyMove(next, move)) continue;

    std::string uci = Chess::squareToAlgebraic(move.from) +
                      Chess::squareToAlgebraic(move.to);
    if (move.promotion >= 2 && move.promotion <= 5) {
      const char promoChars[] = {'\0', '\0', 'n', 'b', 'r', 'q'};
      uci += promoChars[move.promotion];
    }

    std::uint64_t nodes = (depth > 1) ? perft(next, depth - 1) : 1;
    totalNodes += nodes;
    std::cout << uci << ": " << nodes << std::endl;
  }

  auto endTime = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
  if (elapsed == 0) elapsed = 1;
  std::uint64_t nps = (totalNodes * 1000) / static_cast<std::uint64_t>(elapsed);

  std::cout << std::endl;
  std::cout << "Nodes searched: " << totalNodes << std::endl;
  std::cout << "Time: " << elapsed << " ms" << std::endl;
  std::cout << "NPS: " << nps << std::endl;
}

void uciLoop() {
  Board board;
  Search::SearchOptions options;
  std::thread searchThread;

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;

    if (line == "uci") {
      std::cout << "id name Nyx" << std::endl;
      std::cout << "id author Nyx Team" << std::endl;
      std::cout << "option name OwnBook type check default true" << std::endl;
      std::cout << "option name EvalFile type string default nn-nyx.nnue" << std::endl;
      std::cout << "uciok" << std::endl;
    } else if (line == "isready") {
      // Must wait for any pending search to finish before responding
      stopSearchThread(searchThread);
      std::cout << "readyok" << std::endl;
    } else if (line == "ucinewgame") {
      stopSearchThread(searchThread);
      board = Board();
      gGameMovesUCI.clear();
      Search::clearTranspositionTable();
    } else if (line.substr(0, 9) == "setoption") {
      // Parse: setoption name <name> value <value>
      std::istringstream ss(line);
      std::string token, name, value;
      ss >> token; // "setoption"
      ss >> token; // "name"
      ss >> name;
      ss >> token; // "value"
      ss >> value;
      if (name == "OwnBook") {
        Book::setEnabled(value == "true");
      } else if (name == "EvalFile" && !value.empty()) {
        NNUE::init(value);
      }
    } else if (line.substr(0, 8) == "position") {
      // Always stop any running search before updating board
      stopSearchThread(searchThread);

      if (!Chess::setPositionFromUCI(board, line)) {
        std::cout << "info string Failed to parse position command" << std::endl;
      }

      // Extract moves for book probing
      gGameMovesUCI.clear();
      std::size_t movesPos = line.find("moves");
      if (movesPos != std::string::npos && movesPos + 6 < line.size()) {
        gGameMovesUCI = line.substr(movesPos + 6);
      }
    } else if (line.substr(0, 2) == "go") {
      // Always stop any previous search before starting a new one
      stopSearchThread(searchThread);

      // Parse go parameters first
      options = Search::SearchOptions(); // Reset to defaults
      int perftDepth = 0;
      std::istringstream ss(line);
      std::string token;
      ss >> token; // "go"
      while (ss >> token) {
        if (token == "perft") ss >> perftDepth;
        else if (token == "wtime") ss >> options.wtime;
        else if (token == "btime") ss >> options.btime;
        else if (token == "winc") ss >> options.winc;
        else if (token == "binc") ss >> options.binc;
        else if (token == "movestogo") ss >> options.movestogo;
        else if (token == "depth") ss >> options.maxDepth;
        else if (token == "nodes") ss >> options.nodes;
        else if (token == "mate") ss >> options.mate;
        else if (token == "movetime") ss >> options.movetime;
        else if (token == "infinite") options.infinite = true;
      }

      // Handle perft (runs synchronously, no book/search)
      if (perftDepth > 0) {
        dividePerft(board, perftDepth);
        continue;
      }

      // Try opening book
      std::string bookMove = Book::probe(gGameMovesUCI);
      if (!bookMove.empty()) {
        // Validate the book move against the current board
        Board testBoard = board;
        if (Chess::applyUCIMove(testBoard, bookMove)) {
          std::cout << "bestmove " << bookMove << std::endl;
          continue;
        }
        // Book move is illegal for this position — fall through to search
      }

      // Launch search in a background thread
      Board threadBoard = board;
      searchThread = std::thread([threadBoard, options]() {
        Search::SearchResult result = Search::analyzeNextMove(threadBoard, options);
        if (result.hasMove) {
          std::cout << "bestmove " << Search::toUci(result.bestMove) << std::endl;
        } else {
          std::cout << "bestmove 0000" << std::endl;
        }
      });
    } else if (line == "stop") {
      stopSearchThread(searchThread);
    } else if (line == "quit") {
      stopSearchThread(searchThread);
      break;
    }
  }
}
