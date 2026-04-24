#include "search.h"
#include "movegen.h"
#include "eval.h"
#include "utils.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>


namespace Search {

std::atomic<bool> gStopSearch{false};
int gNumThreads = 1;

class TimeManager {
  std::chrono::time_point<std::chrono::steady_clock> startTime;
  long long timeLimitMs;
  bool useTimeManagement;

public:
  void start(const SearchOptions& options, bool whiteTurn) {
    startTime = std::chrono::steady_clock::now();
    useTimeManagement = false;
    timeLimitMs = 0;

    if (options.infinite) return;

    if (options.movetime > 0) {
      timeLimitMs = options.movetime;
      useTimeManagement = true;
      return;
    }

    long long timeLeft = whiteTurn ? options.wtime : options.btime;
    long long inc = whiteTurn ? options.winc : options.binc;

    if (timeLeft > 0) {
      int movesToGo = options.movestogo > 0 ? options.movestogo : 40;
      timeLimitMs = (timeLeft / movesToGo) + (inc / 2);
      if (timeLimitMs > timeLeft) timeLimitMs = timeLeft - 50;
      if (timeLimitMs < 0) timeLimitMs = 0;
      useTimeManagement = true;
    }
  }

  bool timeIsUp() const {
    if (!useTimeManagement) return false;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    return elapsed >= timeLimitMs;
  }

  long long elapsed() const {
     auto now = std::chrono::steady_clock::now();
     return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
  }
};

TimeManager gTimeManager;



enum TTFlag { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

struct TTEntry {
  std::uint64_t key = 0;
  int depth = -1;
  int score = 0;
  int flag = TT_EXACT;
  int bestMoveCode = -1;
};

static constexpr std::size_t TT_SIZE = 1 << 20;
static std::vector<TTEntry> transpositionTable(TT_SIZE);

std::size_t ttIndex(std::uint64_t key) {
  return static_cast<std::size_t>(key & (TT_SIZE - 1));
}

void clearTranspositionTable() {
  for (TTEntry &entry : transpositionTable) {
    entry.key = 0;
    entry.depth = -1;
    entry.score = 0;
    entry.flag = TT_EXACT;
    entry.bestMoveCode = -1;
  }
}

bool probeTranspositionTable(std::uint64_t key, int depth, int alpha, int beta,
                             int &score, int &bestMoveCode,
                             SearchStats &stats) {
  TTEntry &entry = transpositionTable[ttIndex(key)];
  if (entry.key != key)
    return false;

  ++stats.ttHits;
  bestMoveCode = entry.bestMoveCode;

  if (entry.depth < depth)
    return false;

  if (entry.flag == TT_EXACT) {
    score = entry.score;
    ++stats.ttCutoffs;
    return true;
  }
  if (entry.flag == TT_LOWER && entry.score >= beta) {
    score = entry.score;
    ++stats.ttCutoffs;
    return true;
  }
  if (entry.flag == TT_UPPER && entry.score <= alpha) {
    score = entry.score;
    ++stats.ttCutoffs;
    return true;
  }

  return false;
}

void storeTranspositionTable(std::uint64_t key, int depth, int score, int flag,
                             int bestMoveCode, SearchStats &stats) {
  TTEntry &entry = transpositionTable[ttIndex(key)];
  if (entry.key != key || depth >= entry.depth) {
    entry.key = key;
    entry.depth = depth;
    entry.score = score;
    entry.flag = flag;
    entry.bestMoveCode = bestMoveCode;
    ++stats.ttStores;
  }
}

int encodeMove(const Move &move) {
  return move.from | (move.to << 6) | (move.promotion << 12);
}
char promotionToUciChar(int promotionPiece) {
  switch (promotionPiece) {
  case 2:
    return 'n';
  case 3:
    return 'b';
  case 4:
    return 'r';
  case 5:
    return 'q';
  default:
    return '\0';
  }
}

std::string toUci(const Move &move) {
  if (!Chess::isInside(move.from) || !Chess::isInside(move.to))
    return "(none)";

  std::string uci =
      Chess::squareToAlgebraic(move.from) + Chess::squareToAlgebraic(move.to);
  const char promo = promotionToUciChar(move.promotion);
  if (promo != '\0')
    uci.push_back(promo);
  return uci;
}
int staticEvaluateWhite(const Board &board) {
  return Eval::evaluateStatic(board);
}

int evaluateForSideToMove(const Board &board) {
  const int whiteScore = staticEvaluateWhite(board);
  return board.whiteTurn ? whiteScore : -whiteScore;
}
void orderMoves(std::vector<Move> &moves, const Board &board, int ply,
                SearchState &state, SearchStats &stats, int preferredMoveCode) {
  const int sideIndex = board.whiteTurn ? 1 : 0;

  for (Move &move : moves) {
    const int moveCode = encodeMove(move);
    int score = 0;

    if (move.isCapture) {
      const int victimValue = Eval::pieceBaseValue(move.capturedType);
      const int attackerValue = Eval::pieceBaseValue(move.movingType);
      score += 500000 + victimValue * 16 - attackerValue;
      ++stats.mvvLvaOrdered;
    } else {
      if (ply < MAX_PLY) {
        if (state.killer[ply][0] == moveCode) {
          score += 400000;
          ++stats.killerHits;
        } else if (state.killer[ply][1] == moveCode) {
          score += 390000;
          ++stats.killerHits;
        }
      }

      const int historyScore = state.history[sideIndex][move.from][move.to];
      if (historyScore > 0)
        ++stats.historyHits;
      score += historyScore;
    }

    if (move.promotion != 0) {
      score += 250000 + Eval::pieceBaseValue(move.promotion);
    }

    if (moveCode == preferredMoveCode) {
      score += 600000;
    }

    move.orderScore = score;
  }

  std::stable_sort(
      moves.begin(), moves.end(),
      [](const Move &a, const Move &b) { return a.orderScore > b.orderScore; });
}

int quiescence(Board &board, int alpha, int beta, int ply, SearchState &state,
               SearchStats &stats, const SearchOptions &options) {
  ++stats.qNodes;

  const bool inCheck = Chess::isInCheck(board, board.whiteTurn);

  // When in check, we must search all evasions (not just captures)
  if (inCheck) {
    if (ply >= options.quiescenceMaxPly)
      return evaluateForSideToMove(board);

    std::vector<Move> evasions = Chess::generateLegalMoves(board, false);
    if (evasions.empty())
      return -MATE_SCORE + ply;

    orderMoves(evasions, board, ply, state, stats, -1);

    for (const Move &move : evasions) {
      Board next = board;
      if (!Chess::applyMove(next, move))
        continue;

      const int score =
          -quiescence(next, -beta, -alpha, ply + 1, state, stats, options);
      if (score >= beta) {
        ++stats.cutoffs;
        return beta;
      }
      if (score > alpha)
        alpha = score;
    }

    return alpha;
  }

  const int standPat = evaluateForSideToMove(board);
  if (standPat >= beta)
    return beta;
  if (standPat > alpha)
    alpha = standPat;

  if (ply >= options.quiescenceMaxPly)
    return alpha;

  std::vector<Move> captures = Chess::generateLegalMoves(board, true);
  orderMoves(captures, board, ply, state, stats, -1);

  for (const Move &move : captures) {
    Board next = board;
    if (!Chess::applyMove(next, move))
      continue;

    const int score =
        -quiescence(next, -beta, -alpha, ply + 1, state, stats, options);
    if (score >= beta) {
      ++stats.cutoffs;
      return beta;
    }
    if (score > alpha)
      alpha = score;
  }

  return alpha;
}

int negamaxMiniMax(Board &board, int depth, int ply, SearchStats &stats) {


  ++stats.nodes;
  if ((stats.nodes & 2047) == 0) {
    if (gStopSearch || gTimeManager.timeIsUp()) {
      gStopSearch = true;
    }
  }
  if (gStopSearch) return 0;

  if (depth == 0)
    return evaluateForSideToMove(board);
  if (Chess::isFiftyMoveDraw(board) ||
      Chess::isInsufficientMaterialDraw(board) ||
      Chess::isThreefoldRepetitionDraw(board)) {
    return 0;
  }

  std::vector<Move> moves = Chess::generateLegalMoves(board, false);
  if (moves.empty()) {
    return Chess::isInCheck(board, board.whiteTurn) ? (-MATE_SCORE + ply) : 0;
  }

  int bestScore = -INF;
  for (const Move &move : moves) {
    Board next = board;
    if (!Chess::applyMove(next, move))
      continue;
    const int score = -negamaxMiniMax(next, depth - 1, ply + 1, stats);
    if (score > bestScore)
      bestScore = score;
  }

  return bestScore;
}

bool hasNonPawnMaterial(const Board &board, bool white) {
  const std::uint64_t pieces = white ? board.whitePieces : board.blackPieces;
  const std::uint64_t pawns = board.pieceBitboards[white ? 0 : 6];
  const std::uint64_t king = board.pieceBitboards[white ? 5 : 11];
  // Non-pawn material = pieces that are not pawns and not the king
  return (pieces & ~pawns & ~king) != 0;
}

int negamaxAlphaBeta(Board &board, int depth, int alpha, int beta, int ply,
                     SearchState &state, SearchStats &stats,
                     const SearchOptions &options) {
  ++stats.nodes;
  if ((stats.nodes & 2047) == 0) { if (gStopSearch || gTimeManager.timeIsUp()) { gStopSearch = true; } }
  if (gStopSearch) return 0;

  if (Chess::isFiftyMoveDraw(board) ||
      Chess::isInsufficientMaterialDraw(board) ||
      Chess::isThreefoldRepetitionDraw(board)) {
    return 0;
  }

  const int originalAlpha = alpha;
  int ttBestMoveCode = -1;
  int ttScore = 0;
  if (probeTranspositionTable(board.zobristKey, depth, alpha, beta, ttScore,
                              ttBestMoveCode, stats)) {
    return ttScore;
  }

  // Check extension: extend search when in check (max 4 plies of extension)
  const bool inCheck = Chess::isInCheck(board, board.whiteTurn);
  if (inCheck && depth < options.maxDepth + 4) {
    ++depth;
  }

  if (depth == 0) {
    return quiescence(board, alpha, beta, ply, state, stats, options);
  }

  // Null Move Pruning: skip turn test for beta cutoff
  // Conditions: not in check, sufficient depth, not at root, has non-pawn material
  if (!inCheck && depth >= 3 && ply > 0 &&
      hasNonPawnMaterial(board, board.whiteTurn)) {
    Board nullBoard = board;
    nullBoard.whiteTurn = !nullBoard.whiteTurn;
    nullBoard.enPassantSquare = -1;
    nullBoard.zobristKey = Chess::positionHash(nullBoard);

    const int R = 2 + (depth >= 6 ? 1 : 0); // Adaptive reduction
    const int nullScore = -negamaxAlphaBeta(nullBoard, depth - 1 - R,
                                            -beta, -beta + 1, ply + 1,
                                            state, stats, options);
    if (nullScore >= beta) {
      return beta;
    }
  }

  std::vector<Move> moves = Chess::generateLegalMoves(board, false);
  if (moves.empty()) {
    const int terminal =
        Chess::isInCheck(board, board.whiteTurn) ? (-MATE_SCORE + ply) : 0;
    storeTranspositionTable(board.zobristKey, depth, terminal, TT_EXACT, -1,
                            stats);
    return terminal;
  }

  orderMoves(moves, board, ply, state, stats, ttBestMoveCode);

  int bestScore = -INF;
  int bestMoveCode = -1;
  const int sideIndex = board.whiteTurn ? 1 : 0;
  int moveIndex = 0;

  for (const Move &move : moves) {
    Board next = board;
    if (!Chess::applyMove(next, move))
      continue;

    int newDepth = depth - 1;

    // Late Move Reduction (LMR): reduce depth for late quiet moves
    if (moveIndex >= 4 && depth >= 3 && !move.isCapture &&
        move.promotion == 0 && !inCheck) {
      newDepth -= 1;
    }

    int score = -negamaxAlphaBeta(next, newDepth, -beta, -alpha, ply + 1,
                                  state, stats, options);

    // Re-search at full depth if LMR found something interesting
    if (newDepth < depth - 1 && score > alpha) {
      score = -negamaxAlphaBeta(next, depth - 1, -beta, -alpha, ply + 1,
                                state, stats, options);
    }

    if (score > bestScore) {
      bestScore = score;
      bestMoveCode = encodeMove(move);
    }
    if (score > alpha)
      alpha = score;

    if (alpha >= beta) {
      ++stats.cutoffs;

      if (!move.isCapture && ply < MAX_PLY) {
        const int moveCode = encodeMove(move);
        if (state.killer[ply][0] != moveCode) {
          state.killer[ply][1] = state.killer[ply][0];
          state.killer[ply][0] = moveCode;
          ++stats.killerUpdates;
        }

        state.history[sideIndex][move.from][move.to] += depth * depth;
        ++stats.historyUpdates;
      }
      break;
    }

    ++moveIndex;
  }

  int ttFlag = TT_EXACT;
  if (bestScore <= originalAlpha) {
    ttFlag = TT_UPPER;
  } else if (bestScore >= beta) {
    ttFlag = TT_LOWER;
  }
  storeTranspositionTable(board.zobristKey, depth, bestScore, ttFlag,
                          bestMoveCode, stats);

  return bestScore;
}

SearchResult runMiniMax(const Board &rootBoard, int depth) {
  SearchResult result;
  SearchStats stats;

  std::vector<Move> moves = Chess::generateLegalMoves(rootBoard, false);
  if (moves.empty()) {
    result.stats = stats;
    return result;
  }

  int bestScore = -INF;
  Move bestMove;

  for (const Move &move : moves) {
    Board next = rootBoard;
    if (!Chess::applyMove(next, move))
      continue;
    const int score = -negamaxMiniMax(next, depth - 1, 1, stats);

    if (score > bestScore) {
      bestScore = score;
      bestMove = move;
      result.hasMove = true;
    }
  }

  result.bestMove = bestMove;
  result.scoreForSideToMove = bestScore;
  result.scoreForWhite = rootBoard.whiteTurn ? bestScore : -bestScore;
  result.stats = stats;
  return result;
}

SearchResult runAlphaBeta(const Board &rootBoard, int depth, SearchState &state,
                          const SearchOptions &options, int preferredMoveCode) {
  SearchResult result;
  SearchStats stats;

  std::vector<Move> moves = Chess::generateLegalMoves(rootBoard, false);
  orderMoves(moves, rootBoard, 0, state, stats, preferredMoveCode);

  if (moves.empty()) {
    result.stats = stats;
    return result;
  }

  int alpha = -INF;
  const int beta = INF;
  int bestScore = -INF;
  Move bestMove;

  for (const Move &move : moves) {
    Board next = rootBoard;
    if (!Chess::applyMove(next, move))
      continue;

    const int score = -negamaxAlphaBeta(next, depth - 1, -beta, -alpha, 1,
                                        state, stats, options);
    if (score > bestScore) {
      bestScore = score;
      bestMove = move;
      result.hasMove = true;
    }
    if (score > alpha)
      alpha = score;
  }

  result.bestMove = bestMove;
  result.scoreForSideToMove = bestScore;
  result.scoreForWhite = rootBoard.whiteTurn ? bestScore : -bestScore;
  result.stats = stats;
  return result;
}

std::string getPV(const Board& rootBoard, int depth) {
  std::string pvStr = "";
  Board board = rootBoard;
  std::uint64_t key = board.zobristKey;
  
  for (int i = 0; i < depth; ++i) {
    TTEntry &entry = transpositionTable[ttIndex(key)];
    if (entry.key == key && entry.bestMoveCode != -1) {
      int from = entry.bestMoveCode & 0x3F;
      int to = (entry.bestMoveCode >> 6) & 0x3F;
      int promo = (entry.bestMoveCode >> 12) & 0x7;
      Move m; m.from = from; m.to = to; m.promotion = promo;
      
      pvStr += toUci(m) + " ";
      if (promo == 0) {
        if (!Chess::makeMove(board, from, to)) break;
      } else {
        if (!Chess::makeMove(board, from, to, promo)) break;
      }
      key = board.zobristKey;
    } else {
      break;
    }
  }
  return pvStr;
}

SearchResult runIterativeDeepening(const Board &rootBoard, SearchState &state,
                                    const SearchOptions &options,
                                    bool isMainThread = true) {
  if (isMainThread) {
    gStopSearch = false;
    gTimeManager.start(options, rootBoard.whiteTurn);
  }

  SearchResult finalResult;
  SearchStats totalStats;
  int preferredMoveCode = -1;

  static constexpr int ASPIRATION_DELTA = 50;
  int prevScore = 0;
  bool useAspiration = false;

  for (int depth = 1; depth <= options.maxDepth; ++depth) {
    if (gStopSearch) break;

    SearchResult iteration;

    if (useAspiration && depth >= 4) {
      int alpha = prevScore - ASPIRATION_DELTA;
      int beta = prevScore + ASPIRATION_DELTA;

      iteration = runAlphaBeta(rootBoard, depth, state, options, preferredMoveCode);

      if (gStopSearch) break;

      if (iteration.hasMove &&
          (iteration.scoreForSideToMove <= alpha ||
           iteration.scoreForSideToMove >= beta)) {
        iteration = runAlphaBeta(rootBoard, depth, state, options, preferredMoveCode);
      }
    } else {
      iteration = runAlphaBeta(rootBoard, depth, state, options, preferredMoveCode);
    }

    if (gStopSearch) break;
    if (!iteration.hasMove) break;

    finalResult = iteration;
    preferredMoveCode = encodeMove(iteration.bestMove);
    prevScore = iteration.scoreForSideToMove;
    useAspiration = true;

    totalStats.nodes += iteration.stats.nodes;
    totalStats.qNodes += iteration.stats.qNodes;
    totalStats.cutoffs += iteration.stats.cutoffs;
    totalStats.mvvLvaOrdered += iteration.stats.mvvLvaOrdered;
    totalStats.killerHits += iteration.stats.killerHits;
    totalStats.killerUpdates += iteration.stats.killerUpdates;
    totalStats.historyHits += iteration.stats.historyHits;
    totalStats.historyUpdates += iteration.stats.historyUpdates;
    totalStats.ttHits += iteration.stats.ttHits;
    totalStats.ttCutoffs += iteration.stats.ttCutoffs;
    totalStats.ttStores += iteration.stats.ttStores;

    // Only main thread prints info lines
    if (isMainThread) {
      long long elapsed = gTimeManager.elapsed();
      if (elapsed == 0) elapsed = 1;
      long long nps = (totalStats.nodes * 1000) / elapsed;

      std::string pv = getPV(rootBoard, depth);

      std::cout << "info depth " << depth
                << " score cp " << iteration.scoreForSideToMove
                << " nodes " << totalStats.nodes
                << " nps " << nps
                << " time " << elapsed
                << " pv " << pv << std::endl;
    }

    if (options.nodes > 0 && totalStats.nodes >= options.nodes) {
        gStopSearch = true;
        break;
    }
  }

  finalResult.stats = totalStats;
  return finalResult;
}

SearchResult analyzeNextMove(const Board &board, const SearchOptions &options) {
  if (options.enableLogging) {
    std::cout << "[Search] Enabled: AlphaBeta, IterativeDeepening, Quiescence, "
                 "MVV-LVA, Killer, History, CheckExt, NullMove, LMR"
              << '\n';
    if (gNumThreads > 1) {
      std::cout << "[Search] Lazy SMP with " << gNumThreads << " threads" << '\n';
    }
  }

  // === Lazy SMP: spawn helper threads ===
  int numHelpers = gNumThreads - 1; // Thread 0 is this (main) thread
  std::vector<std::thread> helperThreads;
  helperThreads.reserve(numHelpers);

  // Helper threads run their own independent search
  for (int i = 0; i < numHelpers; ++i) {
    helperThreads.emplace_back([&board, &options]() {
      SearchState helperState;
      // Helper threads search silently (isMainThread = false)
      runIterativeDeepening(board, helperState, options, false);
    });
  }

  // Main thread runs the search and reports info lines
  SearchState mainState;
  SearchResult result = runIterativeDeepening(board, mainState, options, true);

  // Signal all helpers to stop and wait for them
  gStopSearch = true;
  for (auto &t : helperThreads) {
    if (t.joinable()) t.join();
  }

  if (options.enableLogging && result.hasMove) {
    std::cout << "[Stats] nodes=" << result.stats.nodes
              << " qnodes=" << result.stats.qNodes
              << " cutoffs=" << result.stats.cutoffs
              << " tthits=" << result.stats.ttHits
              << " ttcutoffs=" << result.stats.ttCutoffs << '\n';
  }

  return result;
}

} // namespace Search
