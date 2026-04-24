/*
 * NNUE Weight Generator for Nyx Chess Engine
 *
 * Creates nn-nyx.nnue that encodes piece values + PSTs directly
 * into a single "material" neuron, producing evaluations consistent
 * with the HCE. The key is proper quantization scaling.
 *
 * Build: c++ -std=c++17 -O2 -o gen_nnue tools/gen_nnue.cpp
 * Usage: ./gen_nnue
 */

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>

// Network dimensions (must match nnue.h)
static constexpr int INPUT_SIZE = 768;
static constexpr int ACC_SIZE = 256;
static constexpr int H1_SIZE = 32;
static constexpr int H2_SIZE = 32;
static constexpr int FT_SCALE = 64;
static constexpr int HIDDEN_SCALE = 64;

// Piece values in centipawns (MG)
static const int PIECE_VALUES[6] = {82, 337, 365, 477, 1025, 0};

// PSTs (from white POV, a1=index 0)
static const int PAWN_PST[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,  10, 10, 10, -10, -10, 10, 10, 10,
    5,  0,  0,  20, 20, 0,  0,  5,  0,  0,  5,  25,  25,  5,  0,  0,
    5,  5,  10, 30, 30, 10, 5,  5,  10, 10, 20, 35,  35,  20, 10, 10,
    40, 40, 40, 45, 45, 40, 40, 40, 0,  0,  0,  0,   0,   0,  0,  0};

static const int KNIGHT_PST[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50, -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30, -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30, -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40, -50,-40,-30,-30,-30,-30,-40,-50};

static const int BISHOP_PST[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20, -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10, -10,  0, 10, 15, 15, 10,  0,-10,
    -10,  5,  5, 15, 15,  5,  5,-10, -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10, -20,-10,-10,-10,-10,-10,-10,-20};

static const int ROOK_PST[64] = {
      0,  0,  5, 10, 10,  5,  0,  0,  -5,  0,  0,  5,  5,  0,  0, -5,
     -5,  0,  0,  5,  5,  0,  0, -5,  -5,  0,  0,  5,  5,  0,  0, -5,
     -5,  0,  0,  5,  5,  0,  0, -5,  -5,  0,  0,  5,  5,  0,  0, -5,
      5, 10, 10, 15, 15, 10, 10,  5,   0,  0,  0,  5,  5,  0,  0,  0};

static const int QUEEN_PST[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20, -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,  -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5, -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10, -20,-10,-10, -5, -5,-10,-10,-20};

static const int KING_PST[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20, -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,  20, 30, 10,  0,  0, 10, 30, 20};

static const int* PST[6] = {PAWN_PST, KNIGHT_PST, BISHOP_PST, ROOK_PST, QUEEN_PST, KING_PST};

static int mirrorSq(int sq) { return (7 - sq / 8) * 8 + sq % 8; }

/*
 * Strategy: Use a SIMPLE architecture that actually works:
 *
 * - Neuron 0 of the accumulator = sum of (piece_value + PST) for all pieces
 *   from this perspective. Friendly pieces add, enemy pieces subtract.
 * - The hidden and output layers simply extract neuron 0 and output it as the
 *   eval in centipawns.
 *
 * This is essentially the HCE material+PST evaluation, but expressed
 * as NNUE weights.
 */

int main() {
  // Feature weights: [768][256]
  std::int16_t featureWeights[INPUT_SIZE][ACC_SIZE] = {};
  std::int16_t featureBiases[ACC_SIZE] = {};

  // For each feature (piece on square), set the weight for neuron 0
  // to encode piece_value + PST_bonus.
  // Feature layout: colorOffset(0=friendly,384=enemy) + pieceType*64 + square
  for (int color = 0; color < 2; ++color) {
    for (int pt = 0; pt < 6; ++pt) {
      for (int sq = 0; sq < 64; ++sq) {
        int featureIdx = color * 384 + pt * 64 + sq;
        int value = PIECE_VALUES[pt] + PST[pt][sq]; // centipawns

        // Friendly pieces contribute positively, enemy negatively
        int sign = (color == 0) ? 1 : -1;

        // Neuron 0: total material+PST eval
        // Scale: we want the accumulator value to represent centipawns directly.
        // The accumulator is int16_t, so we can store up to ±32767.
        // A queen is worth 1025cp, so even with many queens we're fine.
        // No extra scaling needed — store centipawns directly.
        featureWeights[featureIdx][0] = static_cast<std::int16_t>(sign * value);

        // Neurons 1-6: per piece-type material count
        if (pt < 5) {
          featureWeights[featureIdx][1 + pt] = static_cast<std::int16_t>(sign * PIECE_VALUES[pt]);
        }

        // Neurons 7-12: per piece-type PST only
        featureWeights[featureIdx][7 + pt] = static_cast<std::int16_t>(sign * PST[pt][sq]);
      }
    }
  }

  // Hidden1 weights: [512][32]
  // We want to extract the total eval from neuron 0 of the STM accumulator
  // and subtract neuron 0 of the other side accumulator.
  // But wait — both accumulators already encode friendly=positive, enemy=negative
  // from their own perspective. So STM acc neuron 0 already has the material
  // balance from STM's perspective.
  //
  // So we just need hidden layer to pass through acc[0] of the STM side.
  //
  // Input layout: [STM_acc(256), OTHER_acc(256)]
  //
  // The ClippedReLU in evaluate() scales acc by CRELU_MAX/FT_SCALE = 127/64 ≈ 2.
  // So if acc[0] = 100 (centipawns), after CReLU it becomes clamp(100*127/64, 0, 127) = clamp(198, 0, 127) = 127.
  // This means the CReLU saturates quickly. We need to pre-scale down in the feature weights.
  //
  // Actually, let me trace through the math more carefully:
  //   Acc[0] = sum of feature_weights[feat][0] for active features
  //          = net material+PST in centipawns (e.g., starting pos ≈ 0 for equal)
  //
  //   CReLU input = (Acc[0] * 127) / 64  (from nnue.cpp line 148)
  //   CReLU output = clamp(above, 0, 127) → int8_t
  //
  // For starting position: Acc[0] ≈ 0 → CReLU = 0. For white advantage: positive → CReLU caps at 127.
  //
  // The problem: CReLU kills negative values. Both perspectives see ~0 at start,
  // but any slight advantage goes to +127 (max) immediately.
  //
  // Solution: Use TWO neurons per concept — one for positive, one for negative.
  // Neuron 0 = material (positive part), neuron 13 = -material (so it catches negative eval).
  // Then hidden layer does: output = neuron_0_activation - neuron_13_activation.

  // Add the negative mirror neuron
  for (int featureIdx = 0; featureIdx < INPUT_SIZE; ++featureIdx) {
    featureWeights[featureIdx][13] = -featureWeights[featureIdx][0];
  }

  // Also, we need to scale down so that CReLU doesn't saturate for typical values.
  // Typical material difference: 0-500 cp. Scale = 127/64 ≈ 2.
  // So 500cp → 1000 in CReLU input → clamped to 127. We want 500cp → ~60 in CReLU.
  // Scale factor: divide feature weights by 16 so that typical range maps to CReLU range.
  for (int featureIdx = 0; featureIdx < INPUT_SIZE; ++featureIdx) {
    // Scale neurons 0 and 13 by 1/8 to avoid CReLU saturation
    featureWeights[featureIdx][0] /= 8;
    featureWeights[featureIdx][13] /= 8;
  }

  // Hidden1: [512][32] of int8_t
  std::int8_t hidden1Weights[ACC_SIZE * 2][H1_SIZE] = {};
  std::int32_t hidden1Biases[H1_SIZE] = {};

  // Map: hidden1[0] = STM_acc[0] (positive eval from STM perspective)
  //       hidden1[1] = STM_acc[13] (negative eval, stored as positive after CReLU)
  hidden1Weights[0][0] = 64;   // STM positive material → hidden1[0]
  hidden1Weights[13][1] = 64;  // STM negative material → hidden1[1]

  // Hidden2: [32][32] of int8_t — pass through
  std::int8_t hidden2Weights[H2_SIZE][H2_SIZE] = {};
  std::int32_t hidden2Biases[H2_SIZE] = {};
  hidden2Weights[0][0] = 64;  // pass hidden1[0] through
  hidden2Weights[1][1] = 64;  // pass hidden1[1] through

  // Output: [32] of int8_t
  // output = hidden2[0] - hidden2[1]  (positive material minus negative material)
  std::int8_t outputWeights[H2_SIZE] = {};
  outputWeights[0] = 64;   // positive eval
  outputWeights[1] = -64;  // subtract negative eval
  std::int32_t outputBias = 0;

  // === Write file ===
  const char* filename = "nn-nyx.nnue";
  std::ofstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Error: cannot create " << filename << std::endl;
    return 1;
  }

  file.write("NYX1", 4);
  file.write(reinterpret_cast<const char*>(featureWeights), sizeof(featureWeights));
  file.write(reinterpret_cast<const char*>(featureBiases), sizeof(featureBiases));
  file.write(reinterpret_cast<const char*>(hidden1Weights), sizeof(hidden1Weights));
  file.write(reinterpret_cast<const char*>(hidden1Biases), sizeof(hidden1Biases));
  file.write(reinterpret_cast<const char*>(hidden2Weights), sizeof(hidden2Weights));
  file.write(reinterpret_cast<const char*>(hidden2Biases), sizeof(hidden2Biases));
  file.write(reinterpret_cast<const char*>(outputWeights), sizeof(outputWeights));
  file.write(reinterpret_cast<const char*>(&outputBias), sizeof(outputBias));
  file.close();

  // Verify
  std::ifstream check(filename, std::ios::binary | std::ios::ate);
  auto size = check.tellg();
  check.close();

  std::cout << "Generated " << filename << " (" << size << " bytes)" << std::endl;
  std::cout << "Architecture: 768 -> 256 -> 32 -> 32 -> 1" << std::endl;
  std::cout << "Encodes: piece values + PSTs via dual positive/negative neurons" << std::endl;

  return 0;
}
