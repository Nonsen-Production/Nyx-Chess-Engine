#include "book.h"

#include <cstdlib>
#include <ctime>
#include <sstream>

namespace Book {

BookState gBookState;

void init() {
  gBookState.loaded = true;
  gBookState.useBook = true;
  std::srand(static_cast<unsigned>(std::time(nullptr)));
}

void setEnabled(bool enabled) { gBookState.useBook = enabled; }

bool isEnabled() { return gBookState.useBook && gBookState.loaded; }

// Split a string by spaces into tokens
static std::vector<std::string> splitMoves(const std::string& s) {
  std::vector<std::string> tokens;
  std::istringstream iss(s);
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

std::string probe(const std::string& gameMovesUCI) {
  if (!isEnabled()) return "";

  const std::vector<std::string> gameMoves = splitMoves(gameMovesUCI);
  const int ply = static_cast<int>(gameMoves.size());

  // Collect all candidate moves from matching book lines
  struct Candidate {
    std::string move;
    int weight;
  };
  std::vector<Candidate> candidates;

  for (int lineIdx = 0; lineIdx < BUILTIN_BOOK_SIZE; ++lineIdx) {
    const std::vector<std::string> bookMoves =
        splitMoves(BUILTIN_BOOK[lineIdx].moves);

    // Check if current game position matches this line up to current ply
    if (ply >= static_cast<int>(bookMoves.size())) continue;

    bool matches = true;
    for (int i = 0; i < ply; ++i) {
      if (gameMoves[i] != bookMoves[i]) {
        matches = false;
        break;
      }
    }

    if (matches) {
      const std::string& nextMove = bookMoves[ply];

      // Check if we already have this move as a candidate
      bool found = false;
      for (auto& c : candidates) {
        if (c.move == nextMove) {
          c.weight += BUILTIN_BOOK[lineIdx].weight;
          found = true;
          break;
        }
      }
      if (!found) {
        candidates.push_back({nextMove, BUILTIN_BOOK[lineIdx].weight});
      }
    }
  }

  if (candidates.empty()) return "";

  // Weighted random selection
  int totalWeight = 0;
  for (const auto& c : candidates) {
    totalWeight += c.weight;
  }

  int roll = std::rand() % totalWeight;
  for (const auto& c : candidates) {
    roll -= c.weight;
    if (roll < 0) return c.move;
  }

  return candidates[0].move;
}

} // namespace Book
