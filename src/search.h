#pragma once

#include "board.h"
#include "types.h"
#include <vector>
#include <string>

namespace Search {

static constexpr int MAX_PLY = 64;
static constexpr int INF = 1'000'000'000;
static constexpr int MATE_SCORE = 1'000'000;

struct SearchStats {
  long long nodes = 0;
  long long qNodes = 0;
  long long cutoffs = 0;
  long long mvvLvaOrdered = 0;
  long long killerHits = 0;
  long long killerUpdates = 0;
  long long historyHits = 0;
  long long historyUpdates = 0;
  long long ttHits = 0;
  long long ttCutoffs = 0;
  long long ttStores = 0;
};

struct SearchOptions {
  int maxDepth = 4;
  int minimaxDepth = 2;
  int quiescenceMaxPly = 10;
  bool enableLogging = true;
};

struct SearchResult {
  bool hasMove = false;
  Move bestMove;
  int scoreForSideToMove = 0;
  int scoreForWhite = 0;
  SearchStats stats;
};

struct SearchState {
  std::array<std::array<int, 2>, MAX_PLY> killer;
  int history[2][64][64] = {{{0}}};

  SearchState() {
    for (auto &entry : killer) {
      entry[0] = -1;
      entry[1] = -1;
    }
  }
};

void clearTranspositionTable();
std::string toUci(const Move &move);
char promotionToUciChar(int promotionPiece);
SearchResult analyzeNextMove(const Board &board, const SearchOptions &options);

} // namespace Search
