#include "nnue.h"

namespace NNUE {

NNUEWeights gWeights;

bool init(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cout << "info string NNUE file " << filename << " not found, falling back to hand-crafted evaluation." << std::endl;
    gWeights.loaded = false;
    return false;
  }

  // Read and verify magic number
  char magic[4];
  file.read(magic, 4);
  if (magic[0] != 'N' || magic[1] != 'Y' || magic[2] != 'X' ||
      magic[3] != '1') {
    std::cerr << "[NNUE] Invalid magic number in: " << filename << std::endl;
    gWeights.loaded = false;
    return false;
  }

  // Read feature transform weights: [768][256] of int16_t
  file.read(reinterpret_cast<char*>(gWeights.featureWeights),
            sizeof(gWeights.featureWeights));

  // Read feature transform biases: [256] of int16_t
  file.read(reinterpret_cast<char*>(gWeights.featureBiases),
            sizeof(gWeights.featureBiases));

  // Read hidden1 weights: [512][32] of int8_t
  file.read(reinterpret_cast<char*>(gWeights.hidden1Weights),
            sizeof(gWeights.hidden1Weights));

  // Read hidden1 biases: [32] of int32_t
  file.read(reinterpret_cast<char*>(gWeights.hidden1Biases),
            sizeof(gWeights.hidden1Biases));

  // Read hidden2 weights: [32][32] of int8_t
  file.read(reinterpret_cast<char*>(gWeights.hidden2Weights),
            sizeof(gWeights.hidden2Weights));

  // Read hidden2 biases: [32] of int32_t
  file.read(reinterpret_cast<char*>(gWeights.hidden2Biases),
            sizeof(gWeights.hidden2Biases));

  // Read output weights: [32] of int8_t
  file.read(reinterpret_cast<char*>(gWeights.outputWeights),
            sizeof(gWeights.outputWeights));

  // Read output bias: int32_t
  file.read(reinterpret_cast<char*>(&gWeights.outputBias),
            sizeof(gWeights.outputBias));

  if (!file.good()) {
    std::cerr << "[NNUE] Error reading weights from: " << filename
              << std::endl;
    gWeights.loaded = false;
    return false;
  }

  gWeights.loaded = true;
  std::cerr << "[NNUE] Loaded: " << filename << std::endl;
  return true;
}

void computeAccumulator(Accumulator& acc, const int board[64]) {
  if (!gWeights.loaded) {
    acc.valid = false;
    return;
  }

  // Start with biases
  std::copy(std::begin(gWeights.featureBiases),
            std::end(gWeights.featureBiases), acc.white.begin());
  std::copy(std::begin(gWeights.featureBiases),
            std::end(gWeights.featureBiases), acc.black.begin());

  // Add contributions from each piece on the board
  for (int sq = 0; sq < 64; ++sq) {
    const int piece = board[sq];
    if (piece == 0) continue;

    const bool whitePiece = piece > 0;
    const int pieceType = std::abs(piece);

    // White perspective feature
    const int wIdx = featureIndex(true, whitePiece, pieceType, sq);
    for (int i = 0; i < ACCUMULATOR_SIZE; ++i) {
      acc.white[i] += gWeights.featureWeights[wIdx][i];
    }

    // Black perspective feature
    const int bIdx = featureIndex(false, whitePiece, pieceType, sq);
    for (int i = 0; i < ACCUMULATOR_SIZE; ++i) {
      acc.black[i] += gWeights.featureWeights[bIdx][i];
    }
  }

  acc.valid = true;
}

void updateAccumulatorAdd(Accumulator& acc, bool whitePerspective,
                          int featureIdx) {
  auto& side = whitePerspective ? acc.white : acc.black;
  for (int i = 0; i < ACCUMULATOR_SIZE; ++i) {
    side[i] += gWeights.featureWeights[featureIdx][i];
  }
}

void updateAccumulatorSub(Accumulator& acc, bool whitePerspective,
                          int featureIdx) {
  auto& side = whitePerspective ? acc.white : acc.black;
  for (int i = 0; i < ACCUMULATOR_SIZE; ++i) {
    side[i] -= gWeights.featureWeights[featureIdx][i];
  }
}

// Clipped ReLU: clamp value to [0, 127]
static inline std::int8_t clippedRelu(std::int16_t x) {
  if (x < CRELU_MIN) return 0;
  if (x > CRELU_MAX) return static_cast<std::int8_t>(CRELU_MAX);
  return static_cast<std::int8_t>(x);
}

static inline std::int8_t clippedRelu32(std::int32_t x) {
  // After matrix multiply, values are scaled. Divide by HIDDEN_SCALE first.
  const std::int32_t scaled = x / HIDDEN_SCALE;
  if (scaled < CRELU_MIN) return 0;
  if (scaled > CRELU_MAX) return static_cast<std::int8_t>(CRELU_MAX);
  return static_cast<std::int8_t>(scaled);
}

int evaluate(const Accumulator& acc, bool whiteToMove) {
  if (!gWeights.loaded) return 0;

  // Step 1: Apply ClippedReLU to both accumulators and concatenate
  // Order: side-to-move first, then other side
  std::int8_t input1[ACCUMULATOR_SIZE * 2];

  const auto& stmAcc = whiteToMove ? acc.white : acc.black;
  const auto& otherAcc = whiteToMove ? acc.black : acc.white;

  for (int i = 0; i < ACCUMULATOR_SIZE; ++i) {
    // Accumulator values are in FT_SCALE units
    const std::int16_t v1 = (stmAcc[i] * CRELU_MAX) / FT_SCALE;
    input1[i] = clippedRelu(v1);
  }
  for (int i = 0; i < ACCUMULATOR_SIZE; ++i) {
    const std::int16_t v2 = (otherAcc[i] * CRELU_MAX) / FT_SCALE;
    input1[ACCUMULATOR_SIZE + i] = clippedRelu(v2);
  }

  // Step 2: Hidden layer 1 (512 → 32)
  std::int32_t hidden1[HIDDEN1_SIZE];
  for (int j = 0; j < HIDDEN1_SIZE; ++j) {
    std::int32_t sum = gWeights.hidden1Biases[j];
    for (int i = 0; i < ACCUMULATOR_SIZE * 2; ++i) {
      sum += static_cast<std::int32_t>(input1[i]) *
             static_cast<std::int32_t>(gWeights.hidden1Weights[i][j]);
    }
    hidden1[j] = sum;
  }

  // Apply ClippedReLU to hidden1
  std::int8_t input2[HIDDEN1_SIZE];
  for (int i = 0; i < HIDDEN1_SIZE; ++i) {
    input2[i] = clippedRelu32(hidden1[i]);
  }

  // Step 3: Hidden layer 2 (32 → 32)
  std::int32_t hidden2[HIDDEN2_SIZE];
  for (int j = 0; j < HIDDEN2_SIZE; ++j) {
    std::int32_t sum = gWeights.hidden2Biases[j];
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
      sum += static_cast<std::int32_t>(input2[i]) *
             static_cast<std::int32_t>(gWeights.hidden2Weights[i][j]);
    }
    hidden2[j] = sum;
  }

  // Apply ClippedReLU to hidden2
  std::int8_t input3[HIDDEN2_SIZE];
  for (int i = 0; i < HIDDEN2_SIZE; ++i) {
    input3[i] = clippedRelu32(hidden2[i]);
  }

  // Step 4: Output layer (32 → 1)
  std::int32_t output = gWeights.outputBias;
  for (int i = 0; i < HIDDEN2_SIZE; ++i) {
    output += static_cast<std::int32_t>(input3[i]) *
              static_cast<std::int32_t>(gWeights.outputWeights[i]);
  }

  // Convert to centipawns
  // The output is scaled by HIDDEN_SCALE, divide to get centipawns
  const int eval = output / HIDDEN_SCALE;

  // Return from white's perspective
  return whiteToMove ? eval : -eval;
}

int evaluateBoard(const int board[64], bool whiteToMove) {
  Accumulator acc;
  computeAccumulator(acc, board);
  if (!acc.valid) return 0;
  return evaluate(acc, whiteToMove);
}

bool isReady() { return gWeights.loaded; }

} // namespace NNUE
