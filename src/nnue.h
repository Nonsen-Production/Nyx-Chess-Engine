#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>

/*
 * NNUE (Efficiently Updatable Neural Network) for Nyx Chess Engine
 *
 * Architecture: 768 → 256 → 32 → 32 → 1
 *
 * Input features (768):
 *   color(2) × piece_type(6) × square(64) = 768
 *   Encoding: color * 384 + piece_type_index * 64 + square
 *   piece_type_index: 0=Pawn, 1=Knight, 2=Bishop, 3=Rook, 4=Queen, 5=King
 *
 * The accumulator (first hidden layer) is incrementally updated
 * when pieces move, avoiding full recomputation each position.
 *
 * Quantization:
 *   Feature weights/biases: int16_t (scale = 64)
 *   Hidden weights: int8_t (scale = 64)
 *   Hidden biases: int32_t
 *   Output weights: int8_t
 *   Output bias: int32_t
 *
 * Weight file format (.nnue):
 *   4 bytes: magic "NYX1"
 *   768*256*2 bytes: feature weights (int16_t)
 *   256*2 bytes: feature biases (int16_t)
 *   512*32*1 bytes: hidden1 weights (int8_t)
 *   32*4 bytes: hidden1 biases (int32_t)
 *   32*32*1 bytes: hidden2 weights (int8_t)
 *   32*4 bytes: hidden2 biases (int32_t)
 *   32*1 bytes: output weights (int8_t)
 *   1*4 bytes: output bias (int32_t)
 */

namespace NNUE {

// Network dimensions
static constexpr int INPUT_SIZE = 768;
static constexpr int ACCUMULATOR_SIZE = 256;
static constexpr int HIDDEN1_SIZE = 32;
static constexpr int HIDDEN2_SIZE = 32;
static constexpr int OUTPUT_SIZE = 1;

// Quantization scales
static constexpr int FT_SCALE = 64;   // Feature transform scale
static constexpr int HIDDEN_SCALE = 64; // Hidden layer scale
static constexpr int OUTPUT_SCALE = FT_SCALE * HIDDEN_SCALE; // Combined

// ClippedReLU bounds
static constexpr int CRELU_MIN = 0;
static constexpr int CRELU_MAX = 127;

// Accumulator: holds the output of the first hidden layer for one perspective
struct Accumulator {
  std::array<std::int16_t, ACCUMULATOR_SIZE> white;
  std::array<std::int16_t, ACCUMULATOR_SIZE> black;
  bool valid = false;
};

// Network weights (global, loaded once)
struct NNUEWeights {
  // Feature transform: input(768) → accumulator(256)
  std::int16_t featureWeights[INPUT_SIZE][ACCUMULATOR_SIZE];
  std::int16_t featureBiases[ACCUMULATOR_SIZE];

  // Hidden layer 1: 512 → 32 (512 = 2 × 256 concatenated accumulators)
  std::int8_t hidden1Weights[ACCUMULATOR_SIZE * 2][HIDDEN1_SIZE];
  std::int32_t hidden1Biases[HIDDEN1_SIZE];

  // Hidden layer 2: 32 → 32
  std::int8_t hidden2Weights[HIDDEN2_SIZE][HIDDEN2_SIZE];
  std::int32_t hidden2Biases[HIDDEN2_SIZE];

  // Output layer: 32 → 1
  std::int8_t outputWeights[HIDDEN2_SIZE];
  std::int32_t outputBias;

  bool loaded = false;
};

// Global weights instance
extern NNUEWeights gWeights;

// Feature index computation
// From a given perspective (white or black), compute the feature index
// for a piece of given color and type on a given square.
inline int featureIndex(bool whitePerspective, bool whitePiece,
                        int pieceType, int square) {
  // pieceType: 1=Pawn..6=King (engine convention)
  const int typeIdx = pieceType - 1; // 0-5

  if (whitePerspective) {
    // White perspective: white pieces at indices 0-383, black at 384-767
    const int colorOffset = whitePiece ? 0 : 384;
    return colorOffset + typeIdx * 64 + square;
  } else {
    // Black perspective: mirror the board (flip ranks) and swap colors
    const int mirrorSq = (7 - (square / 8)) * 8 + (square % 8);
    const int colorOffset = whitePiece ? 384 : 0; // swap: "their" pieces go second
    return colorOffset + typeIdx * 64 + mirrorSq;
  }
}

// Initialize weights from a .nnue file
bool init(const std::string& filename);

// Compute accumulator from scratch for a given board position
// board[64]: piece values (positive=white, negative=black, 0=empty)
// abs(piece): 1=Pawn..6=King
void computeAccumulator(Accumulator& acc, const int board[64]);

// Incrementally update accumulator after a piece moves
// Call with the features that were removed and added
void updateAccumulatorAdd(Accumulator& acc, bool whitePerspective,
                          int featureIdx);
void updateAccumulatorSub(Accumulator& acc, bool whitePerspective,
                          int featureIdx);

// Run the full forward pass from the accumulator to get evaluation
// whiteToMove: whose turn it is (affects which accumulator comes first)
// Returns evaluation in centipawns from white's perspective
int evaluate(const Accumulator& acc, bool whiteToMove);

// Convenience: full evaluation from board array
int evaluateBoard(const int board[64], bool whiteToMove);

// Check if NNUE is loaded and ready
bool isReady();

} // namespace NNUE
