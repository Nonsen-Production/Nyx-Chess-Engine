#pragma once

#include "board.h"
#include "types.h"
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>

namespace Search {

extern std::atomic<bool> gStopSearch;
extern int gNumThreads;  // Number of search threads (Lazy SMP)

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
  int maxDepth = 64;
  int minimaxDepth = 2;
  int quiescenceMaxPly = 10;
  bool enableLogging = false;

  bool infinite = false;
  long long wtime = 0;
  long long btime = 0;
  long long winc = 0;
  long long binc = 0;
  long long movetime = 0;
  int movestogo = 0;
  long long nodes = 0;
  int mate = 0;
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
  int counterMove[2][64][64] = {{{0}}};

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
