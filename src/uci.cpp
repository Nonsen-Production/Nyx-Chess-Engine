#include "uci.h"
#include "utils.h"
#include "movegen.h"
#include "search.h"
#include "book.h"
#include "nnue.h"
#include <iostream>
#include <sstream>

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

void uciLoop() {
  Board board;
  Search::SearchOptions options;
  options.maxDepth = 8;
  options.minimaxDepth = 2;
  options.quiescenceMaxPly = 12;
  options.enableLogging = false;

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "uci") {
      std::cout << "id name Nyx" << std::endl;
      std::cout << "id author Nyx Team" << std::endl;
      std::cout << "option name OwnBook type check default true" << std::endl;
      std::cout << "option name EvalFile type string default nn-nyx.nnue" << std::endl;
      std::cout << "uciok" << std::endl;
    } else if (line == "isready") {
      std::cout << "readyok" << std::endl;
    } else if (line == "ucinewgame") {
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
      Chess::setPositionFromUCI(board, line);

      // Extract moves for book probing
      gGameMovesUCI.clear();
      std::size_t movesPos = line.find("moves");
      if (movesPos != std::string::npos && movesPos + 6 < line.size()) {
        gGameMovesUCI = line.substr(movesPos + 6);
      }
    } else if (line.substr(0, 2) == "go") {
      // Try opening book first
      std::string bookMove = Book::probe(gGameMovesUCI);
      if (!bookMove.empty()) {
        std::cout << "bestmove " << bookMove << std::endl;

        // Apply book move to internal state for next probe
        Chess::applyUCIMove(board, bookMove);
        if (!gGameMovesUCI.empty()) gGameMovesUCI += " ";
        gGameMovesUCI += bookMove;
      } else {
        // Run search
        Search::SearchResult result = Search::analyzeNextMove(board, options);
        if (result.hasMove) {
          std::cout << "bestmove " << Search::toUci(result.bestMove) << std::endl;
        }
      }
    } else if (line == "quit") {
      break;
    }
  }

}
